import requests
import json
import os
import argparse
import webbrowser
from flask import Flask, request

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
REDIRECT_URI = "http://203.28.176.50:8123/oauth/callback"

def get_refresh_token():
    """Handles first-time OAuth setup by launching a webserver to capture the authorization code."""
    auth_url = f"{AUTH_URL}?client_id={CLIENT_ID}&response_type=code&redirect_uri={REDIRECT_URI}"
    print("Please visit the following URL in your browser and authorize access:")
    print(auth_url)
    webbrowser.open(auth_url)
    print("Waiting for authentication response...")

app = Flask(__name__)

@app.route("/oauth/callback")
def oauth_callback():
    """Handles OAuth callback and exchanges authorization code for a refresh token."""
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
        print("OAuth setup complete. Refresh token saved.")
        return "Authentication successful. You may close this window."
    else:
        return f"Error fetching token: {response.text}"

def start_webserver():
    """Start a Flask webserver to handle OAuth callback."""
    print("Starting OAuth callback server on 203.28.176.50:8123...")
    app.run(host="203.28.176.50", port=8123)

def get_access_token():
    """Fetch a new access token using the refresh token, and update the refresh token if provided."""
    refresh_token = read_secret(REFRESH_TOKEN_FILE)
    if not refresh_token:
        print("No refresh token found. Starting OAuth webserver...")
        start_webserver()
        refresh_token = read_secret(REFRESH_TOKEN_FILE)
    
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
