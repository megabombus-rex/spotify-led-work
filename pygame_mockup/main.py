import io

from PIL import Image
import requests

from image_edit.image import convert_image_to_grid, resize_image_plain
from spotify_wrappers.spotify_headless import get_access_token, get_current_song

import pygame

LED_CIRCLE_RADIUS = 25
IDENT_SIZE = 2 * LED_CIRCLE_RADIUS
MATRIX_SIZE = 16

# call_delay_ms = 15000 # 15 seconds
call_delay_ms = 5000 # 15 seconds
call_event = pygame.USEREVENT + 1
speed = 1.0 

def check_current_song(access_token: str):
    print("Calling Spotify API...")
    song_id, name, image_url = get_current_song(access_token)

    return song_id, name, image_url

if __name__ == '__main__':

    
    # 8x8 will be used for LED display, start with all red
    grid = [[(255, 0, 0) for _ in range(MATRIX_SIZE)] for _ in range(MATRIX_SIZE)]

    saved_image = None
    saved_song_id = None

    access_token = get_access_token()
    
    # colors = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (255, 0, 255), (0, 255, 255), (128, 128, 128), (255, 165, 0)]
    
    pygame.init()
    
    window = pygame.display.set_mode((800, 800))
    window.fill((0, 0, 0))
    pygame.display.set_caption("Spotify LED Mockup")


    pygame.time.set_timer(call_event, call_delay_ms)
    
    running = True
    while running:

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

            if event.type == call_event:
                try:
                    current_song_id, name, image_url = check_current_song(access_token)
                    if current_song_id is None or name is None or image_url is None:
                        access_token = get_access_token()
                        current_song_id, name, image_url = check_current_song(access_token)
                        
                    print(f"Current song: {name} (ID: {current_song_id})")
                    
                    if  current_song_id != saved_song_id:
                            img = requests.get(image_url, stream=True).content
                            resize_image_plain(Image.open(io.BytesIO(img)), "images/current_song.png", (MATRIX_SIZE, MATRIX_SIZE))
                            
                            grid = convert_image_to_grid("images/current_song.png")
                except Exception as e:
                    print(f"Error processing song data: {e}")
                
        for i in range(MATRIX_SIZE):
            for j in range(MATRIX_SIZE):
                
                y = (j * IDENT_SIZE) + LED_CIRCLE_RADIUS
                x = (i * IDENT_SIZE) + LED_CIRCLE_RADIUS
                # color = random.choice(colors)
                # color = (255, 0, 0)
                pygame.draw.circle(window, grid[j][i], (x, y), LED_CIRCLE_RADIUS)
        pygame.display.update()        

    # Quit Pygame
    pygame.quit()
    