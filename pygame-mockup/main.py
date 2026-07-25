import os
import random
from dotenv import load_dotenv

import pygame

LED_CIRCLE_RADIUS = 50
IDENT_SIZE = 2 * LED_CIRCLE_RADIUS
MATRIX_SIZE = 8

if __name__ == '__main__':
    # used for spotify api key
    load_dotenv()
    
    # 8x8 will be used for LED display
    grid = [[] for _ in range(MATRIX_SIZE)]
    
    # colors = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (255, 0, 255), (0, 255, 255), (128, 128, 128), (255, 165, 0)]
    
    pygame.init()
    
    window = pygame.display.set_mode((800, 800))
    window.fill((255, 255, 255))
    pygame.display.set_caption("Spotify LED Mockup")
    
    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
                
        for i in range(MATRIX_SIZE):
            for j in range(MATRIX_SIZE):
                
                y = (j * IDENT_SIZE) + LED_CIRCLE_RADIUS
                x = (i * IDENT_SIZE) + LED_CIRCLE_RADIUS
                # color = random.choice(colors)
                color = (255, 0, 0)
                pygame.draw.circle(window, color, (x, y), LED_CIRCLE_RADIUS)
        pygame.display.update()        

    # Quit Pygame
    pygame.quit()
    