from PIL import Image

def resize_image(image_path, output_path, size):
    """
    Resize an image to the specified size and save it to the output path.

    :param image_path: Path to the input image.
    :param output_path: Path where the resized image will be saved.
    :param size: Tuple specifying the new size (width, height).
    """
    with Image.open(image_path) as img:
        img = img.resize(size)
        img.save(output_path)

def resize_image_plain(img_data, output_path, size):
        img = img_data.resize(size)
        img.save(output_path)

def convert_image_to_grid(image_path):
    """
    Convert an image to a grid of RGB tuples.

    :param image_path: Path to the input image.
    :return: A 2D list representing the grid of RGB tuples.
    """
    with Image.open(image_path) as img:
        img = img.convert("RGB")
        width, height = img.size
        grid = [[img.getpixel((x, y)) for x in range(width)] for y in range(height)]
    return grid

if __name__ == "__main__":
    print("This is the image-edit module.")

    # path = "images/big_yoshi_400x400.jpg"
    # output_path = "images/big_yoshi_8x8.jpg"
    path = "images/clown_big.png"
    output_path = "images/clown_8x8.png"
    # path = "images/big_yoshi_400x400.jpg"
    # output_path = "images/big_yoshi_8x8.jpg"
    # path = "images/big_yoshi_400x400.jpg"
    # output_path = "images/big_yoshi_8x8.jpg"
    resize_image(path, output_path, (8, 8))