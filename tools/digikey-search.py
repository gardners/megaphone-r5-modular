import json
import requests
import time
import os
import webbrowser
import argparse
from flask import Flask, request
from threading import Thread

# DigiKey API Details
AUTH_URL = "https://api.digikey.com/v1/oauth2/authorize"
TOKEN_URL = "https://api.digikey.com/v1/oauth2/token"
SEARCH_URL = "https://api.digikey.com/products/v4/search/{}/productdetails"


# Local OAuth Server Configuration
REDIRECT_URI = "https://oauth.horseomatic.org:8123/oauth/callback"
CERT_FILE = "secrets/fullchain.pem"
KEY_FILE = "secrets/server.pem"

# File paths for secrets
CLIENT_ID_FILE = "secrets/digikey-client-id"
CLIENT_SECRET_FILE = "secrets/digikey-client-secret"
REFRESH_TOKEN_FILE = "secrets/digikey-refresh-token"

# Flask App
app = Flask(__name__)
auth_success = False  # Global flag to track authentication status

### Utility Functions ###
def read_secret(file_path):
    """Reads a secret from a file, stripping spaces and newlines."""
    try:
        with open(file_path, "r") as f:
            return f.read().strip()
    except FileNotFoundError:
        return None

# Load credentials
CLIENT_ID = read_secret(CLIENT_ID_FILE)
CLIENT_SECRET = read_secret(CLIENT_SECRET_FILE)

### OAuth Handling ###
@app.route("/oauth/callback")
def oauth_callback():
    """Handles OAuth callback and exchanges authorization code for a refresh token."""
    global auth_success
    code = request.args.get("code")
    if not code:
        return "Error: No authorization code received."

    token_data = {
        "client_id": CLIENT_ID,
        "client_secret": CLIENT_SECRET,
        "code": code,
        "grant_type": "authorization_code",
        "redirect_uri": REDIRECT_URI,
    }

    response = requests.post(TOKEN_URL, data=token_data)
    
    if response.status_code == 200:
        token_info = response.json()
        refresh_token = token_info.get("refresh_token")
        if refresh_token:
            with open(REFRESH_TOKEN_FILE, "w") as f:
                f.write(refresh_token)
            auth_success = True  # Signal authentication success
        print("OAuth setup complete. Refresh token saved.")
        return "Authentication successful. You may close this window."
    else:
        return f"Error fetching token: {response.text}"

def start_webserver():
    """Starts a Flask webserver to handle OAuth callback using HTTPS in a background thread."""
    server_thread = Thread(target=lambda: app.run(host="0.0.0.0", port=8123, ssl_context=(CERT_FILE, KEY_FILE)), daemon=True)
    server_thread.start()

def get_refresh_token():
    """Handles first-time OAuth setup by launching a webserver to capture the authorization code."""
    global auth_success
    auth_success = False  # Reset flag before starting

    auth_url = f"{AUTH_URL}?client_id={CLIENT_ID}&response_type=code&redirect_uri={REDIRECT_URI}"
    
    print("Please visit the following URL in your browser and authorize access:")
    print(auth_url)
    webbrowser.open(auth_url)

    print("Starting OAuth callback server on oauth.horseomatic.org:8123 with HTTPS...")
    start_webserver()

    print("Waiting for authentication response...")

    # Periodically check for the refresh token file
    timeout = 180  # Timeout in seconds (3 minutes)
    start_time = time.time()

    while time.time() - start_time < timeout:
        if auth_success or (os.path.exists(REFRESH_TOKEN_FILE) and os.path.getsize(REFRESH_TOKEN_FILE) > 0):
            print("OAuth successful. Refresh token obtained.")
            return
        time.sleep(2)  # Poll every 2 seconds

    print("OAuth timed out. Exiting.")
    os._exit(1)

### Access Token Handling ###
def get_access_token():
    """Fetch a new access token using the refresh token, and update the refresh token if needed."""
    refresh_token = read_secret(REFRESH_TOKEN_FILE)
    if not refresh_token:
        print("No refresh token found. Starting OAuth flow...")
        get_refresh_token()
        refresh_token = read_secret(REFRESH_TOKEN_FILE)  # Reload after obtaining

    token_data = {
        "client_id": CLIENT_ID,
        "client_secret": CLIENT_SECRET,
        "refresh_token": refresh_token,
        "grant_type": "refresh_token"
    }
    
    response = requests.post(TOKEN_URL, data=token_data)
    
    if response.status_code == 200:
        token_info = response.json()
        access_token = token_info.get("access_token")
        new_refresh_token = token_info.get("refresh_token")
        
        if new_refresh_token:
            with open(REFRESH_TOKEN_FILE, "w") as f:
                f.write(new_refresh_token)
        
        return access_token
    else:
        print("Error refreshing access token:", response.text)
        os._exit(1)

### DigiKey Search Function ###
def search_digikey(mpn):
    """Search DigiKey for the given Manufacturer Part Number (MPN)."""
    access_token = get_access_token()
    headers = {
       "Authorization": f"Bearer {access_token}",
       "Content-Type": "application/json",
       "X-DIGIKEY-Client-Id": CLIENT_ID,
       "Accept": "application/json"
    }

    headers = {
        "Authorization": f"Bearer {access_token}",
        "Content-Type": "application/json",
        "X-DIGIKEY-Client-Id": CLIENT_ID
    }
    payload = {
        "Keywords": mpn,
        "RecordCount": 10,
        "Filters": {
            "InStock": True, 
            "NormallyStocked": True, 
            "ExcludeMarketPlace": True, 
            "ExcludeObsolete": True
        }
    }

    url = SEARCH_URL.format(mpn)  # Correct URL formatting
    response = requests.get(url, headers=headers, json=payload)

    print("API Request URL:", url)
    print("Request Headers:", headers)
    print("Request Payload:", json.dumps(payload, indent=2))
    print("Response Status Code:", response.status_code)
    print("Response Body:", response.text)    
    
    if response.status_code == 200:
        results = response.json()
        if "Parts" in results and results["Parts"]:
            best_match = results["Parts"][0]  # Take the first result
            return best_match["DigiKeyPartNumber"], best_match["ProductUrl"]
        else:
            print("No valid parts found on DigiKey.")
            return None, None
    else:
        print("Error searching DigiKey:", response.text)
        return None, None

### Main Function ###
def main():
    parser = argparse.ArgumentParser(description="Search DigiKey for a Manufacturer's Part Number (MPN).")
    parser.add_argument("mpn", help="Manufacturer's Part Number to search for.")
    args = parser.parse_args()
    
    part_number, product_url = search_digikey(args.mpn)
    
    if part_number and product_url:
        with open("part-info.tmp", "w") as f:
            f.write(f"{part_number},{product_url}\n")
        print(f"Part saved: {part_number}, URL: {product_url}")
    else:
        if os.path.exists("part-info.tmp"):
            os.remove("part-info.tmp")
        print("No part selected.")

### Script Entry Point ###
if __name__ == "__main__":
    main()
