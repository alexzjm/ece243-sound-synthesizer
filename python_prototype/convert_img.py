# this file is for converting the image to the format of c-style array

import numpy as np
import cv2
import pyperclip


def convert_img(img_path, img_name, width, height):
    img = cv2.imread(img_path)
    img = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    img = cv2.resize(img, (width, height))
    img = img / 255
    img = np.round(img)
    img = img.astype(np.int8)
    img = img.flatten()
    img = img.tolist()
    img = ', '.join([str(i) for i in img])
    # add the beginning and the end of the array
    img = f"int8_t {img_name}[{width * height}] = {{" + img + "};"
    # add the draw_image function
    img += "\n\n"
    img += generate_draw_pixel_code(img_name, width, height, 255)
    pyperclip.copy(img)


def generate_draw_pixel_code(img_name, width, height, color):
    code_lines = []
    for y in range(height):
        for x in range(width):
            code_lines.append(f"    draw_pixel({x}, {y}, {color} * {img_name}[{y * width + x}]);")
    code = "\n".join(code_lines)
    c_code = f"void draw_image() {{\n{code}\n}}"
    return c_code


if __name__ == '__main__':
    convert_img('images/sine.png', 'sine_img', 20, 10)