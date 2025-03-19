# this file is for converting the image to the format of c-style array

import numpy as np
import cv2
import pyperclip


# this function converts the image to the c-style array
# the image is first resized to the specified width and height
# the image is then converted to short unsigned int color with 5 bits for red, 6 bits for green, and 5 bits for blue
# the image is then flattened to a 1D array
# the image is then copied to the clipboard
def convert_img(file_name, img_name, width, height):
    img = cv2.imread(file_name).astype(np.uint16)
    img = cv2.resize(img, (width, height), interpolation=cv2.INTER_NEAREST)
    # img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    img_r = img[:, :, 2] >> 3
    img_g = img[:, :, 1] >> 2
    img_b = img[:, :, 0] >> 3
    print(img_r[0, 0], img_g[0, 0], img_b[0, 0])
    img = (img_r << 11) + (img_g << 5) + img_b
    print(img[0, 0])
    img = img.flatten()
    answer = ', '.join(map(lambda x: f"0x{x:04X}", img))
    answer = f"short unsigned int {img_name}[{width * height}] = {{{answer}}};"

    answer += "\n\n"
    answer += generate_draw_pixel_code(img_name, width, height, img_name)

    pyperclip.copy(answer)


# This is the function that generates the plot_image function
# this function utilizes the plot_pixel function
# the signature of the plot_pixel function is assumed to be void plot_pixel(int x, int y, short unsigned int color);
# the signature of the plot_image function is assumed to be void plot_image_img_name(int x, int y);
def generate_draw_pixel_code(img_name, width, height, color):
    code = f"""
void plot_image_{img_name}(int x, int y) {{
    for (int i = 0; i < {height}; i++) {{
        for (int j = 0; j < {width}; j++) {{
            plot_pixel(x + j, y + i, {color}[i * {width} + j]);
        }}
    }}
}}


void erase_image_{img_name}(int x, int y) {{
    for (int i = 0; i < {height}; i++) {{
        for (int j = 0; j < {width}; j++) {{
            plot_pixel(x + j, y + i, 0);
        }}
    }}
}}
"""
    return code


if __name__ == '__main__':
    convert_img(
        file_name='images/sine.png',
        img_name='sine',
        width=50,
        height=20
    )
