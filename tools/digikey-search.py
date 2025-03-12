import requests
import json
import os
import argparse
import webbrowser
import threading
import time
from flask import Flask, request
import ssl

# Read DigiKey credentials from files, stripping spaces and newlines
def read_secret(file_path):
    try:
        with open(file_path, "r") as f:
            return f.read().strip()
    except FileNotFoundError:
        return None

CLIENT_ID = read_secret("secrets/digikey-client-id")
CLIENT_SECRET = read_secret("secrets/digikey-client-secret")
REFRESH_TOKEN_FILE = "secrets/digikey-refresh-token"
TOKEN_URL = "https://api.digikey.com/v1/oauth2/token"
SEARCH_URL = "https://api.digikey.com/services/api/v1/search/keyword"
AUTH_URL = "https://api.digikey.com/v1/oauth2/authorize"
REDIRECT_URI = "https://oauth.horseomatic.org:8123/oauth/callback"
CERT_FILE = "secrets/fullchain.pem"
KEY_FILE = "secrets/server.pem"

app = Flask(__name__)
auth_failed = False  # Track if the OAuth request failed

@app.route("/oauth/callback")
def oauth_callback():
    """Handles OAuth callback and exchanges authorization code for a refresh token."""
    global auth_failed
    code = request.args.get("code")
    if not code:
        auth_failed = True
        return "Error: No authorization code received."

    token_data = {
        "client_id": CLIENT_ID,
        "client_secret": CLIENT_SECRET,
        "code": code,
        "grant_type": "authorization_code",
        "redirect_uri": REDIRECT_URI,
    }

    response = requests.post(TOKEN_URL, data=token_data)
    print("OAuth Response Status Code:", response.status_code)
    print("OAuth Response Body:", response.text)

    if response.status_code == 200:
        token_info = response.json()
        refresh_token = token_info.get("refresh_token")
        if refresh_token:
            with open(REFRESH_TOKEN_FILE, "w") as f:
                f.write(refresh_token)
        print("OAuth setup complete. Refresh token saved.")
        return "Authentication successful. You may close this window."
    else:
        auth_failed = True
        return f"Error fetching token: {response.text}"

def start_webserver():
    """Start a Flask webserver in a separate thread to handle OAuth callback using HTTPS."""
    def run_server():
        print("Starting OAuth callback server on oauth.horseomatic.org:8123 with HTTPS...")
        app.run(host="0.0.0.0", port=8123, ssl_context=(CERT_FILE, KEY_FILE))

    server_thread = threading.Thread(target=run_server, daemon=True)
    server_thread.start()
    return server_thread

def get_refresh_token():
    """Handles first-time OAuth setup by launching a webserver to capture the authorization code."""
    auth_url = f"{AUTH_URL}?client_id={CLIENT_ID}&response_type=code&redirect_uri={REDIRECT_URI}"
    
    # Make a test request to check if the redirect URI is valid before proceeding
    test_response = requests.get(auth_url, allow_redirects=False)
    
    if test_response.status_code != 302:  # 302 indicates a redirect is allowed
        print(f"Error: OAuth request failed with status {test_response.status_code}")
        print(f"Response: {test_response.text}")
        print("Check that the redirect URI is correctly registered with DigiKey.")
        exit(1)

    print("OAuth request successful, proceeding with authentication...")
    print("Please visit the following URL in your browser and authorize access:")
    print(auth_url)
    webbrowser.open(auth_url)

    print("Starting OAuth callback server on oauth.horseomatic.org:8123 with HTTPS...")
    start_webserver()

    print("Waiting for authentication response...")

    # Periodically check for failure
    timeout = 180  # Timeout in seconds (3 minutes)
    start_time = time.time()

    while time.time() - start_time < timeout:
        if auth_failed:
            print("OAuth failed. Exiting.")
            os._exit(1)  # Force immediate exit
        time.sleep(2)  # Poll every 2 seconds

    print("OAuth timed out. Exiting.")
    os._exit(1)

def get_access_token():
    """Fetch a new access token using the refresh token, and update the refresh token if provided."""
    refresh_token = read_secret(REFRESH_TOKEN_FILE)
    if not refresh_token:
        print("No refresh token found. Starting OAuth flow...")
        get_refresh_token()
        refresh_token = read_secret(REFRESH_TOKEN_FILE)
    
    token_data = {
        "client_id": CLIENT_ID,
        "client_secret": CLIENT_SECRET,
        "refresh_token": refresh_token,
        "grant_type": "refresh_token"
    }
    
    response = requests.post(TOKEN_URL, data=token_data)
    print("Token Refresh Response Status Code:", response.status_code)
    print("Token Refresh Response Body:", response.text)
    
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
        exit(1)

def search_digikey(mpn):
    """Search DigiKey for the given Manufacturer Part Number (MPN)."""
    access_token = get_access_token()
    headers = {
        "Authorization": f"Bearer {access_token}",
        "Content-Type": "application/json",
        "X-DIGIKEY-Client-Id": CLIENT_ID
    }
    payload = {
        "Keywords": mpn,
        "RecordCount": 10,
        "Filters": {"InStock": True, "NormallyStocked": True, "ExcludeMarketPlace": True, "ExcludeObsolete": True}
    }

    response = requests.post(SEARCH_URL, headers=headers, json=payload)
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

if __name__ == "__main__":
    main()
