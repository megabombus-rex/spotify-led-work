from http.server import BaseHTTPRequestHandler, HTTPServer
import os
import webbrowser
import requests
from urllib.parse import urlparse, parse_qs

from dotenv import load_dotenv

load_dotenv()

CLIENT_SECRET = os.getenv("SPOTIFY_CLIENT_SECRET")
CLIENT_ID = os.getenv("SPOTIFY_CLIENT_ID")
REDIRECT_URI = "http://127.0.0.1:8888/callback"
SCOPE = "user-read-currently-playing user-read-playback-state"

auth_code = None

class CallbackHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        global auth_code
        query = parse_qs(urlparse(self.path).query)
        auth_code = query.get("code", [None])[0]
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b"You can close this tab now.")

def get_refresh_token():
    uri = f"https://accounts.spotify.com/authorize?client_id={CLIENT_ID}&response_type=code&redirect_uri={REDIRECT_URI}&scope={SCOPE.replace(' ', '%20')}"
    webbrowser.open(uri)
    server = HTTPServer(("127.0.0.1", 8888), CallbackHandler)
    server.handle_request()

    resp = requests.post(
        "https://accounts.spotify.com/api/token",
        data={
            "grant_type": "authorization_code",
            "code": auth_code,
            "redirect_uri": REDIRECT_URI,
            "client_id": CLIENT_ID,
            "client_secret": CLIENT_SECRET,
        },
    )
    print(resp.json())
    return resp.json()["access_token"]

if __name__ == "__main__":
    token = get_refresh_token()