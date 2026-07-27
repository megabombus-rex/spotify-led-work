import os

from dotenv import load_dotenv
import requests

load_dotenv()

CLIENT_SECRET = os.getenv("SPOTIFY_CLIENT_SECRET")
CLIENT_ID = os.getenv("SPOTIFY_CLIENT_ID")
REFRESH_TOKEN = os.getenv("SPOTIFY_REFRESH_TOKEN")

def get_access_token():
    resp = requests.post(
        "https://accounts.spotify.com/api/token",
        data={
            "grant_type": "refresh_token",
            "refresh_token": REFRESH_TOKEN,
            "client_id": CLIENT_ID,
            "client_secret": CLIENT_SECRET,
        },
    )
    return resp.json()["access_token"]

def get_current_song(access_token):
    resp = requests.get(
        "https://api.spotify.com/v1/me/player/currently-playing",
        headers={"Authorization": f"Bearer {access_token}"},
    )
    if resp.status_code == 204:
        return None
    data = resp.json()
    item = data["item"]
    return item["id"], item["name"], item["album"]["images"][0]["url"]

if __name__ == "__main__":
    token = get_access_token()
    print(get_current_song(token))
