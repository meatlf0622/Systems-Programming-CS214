import os
import numpy as np
import argparse
import ctypes
import cv2
import time
from PIL import Image

# Run using the following command:
# python convertRGB_YCbCr.py -i input.jpg -o output.jpg -e 1

# def rgb_to_ycbcr(r, g, b):
#     """
#     Convert an RGB pixel to YCbCr using the Wikipedia formula.
#     """
#     y  = int( 0.299 * r + 0.587 * g + 0.114 * b)
#     cb = int(128 + (-0.168736 * r - 0.331264 * g + 0.5 * b))
#     cr = int(128 + (0.5 * r - 0.418688 * g - 0.081312 * b))
    
#     return max(0, min(255, y)), max(0, min(255, cb)), max(0, min(255, cr))

# use this to run 

# this function loads the C shared object library 
def load_library():
    try:
        return ctypes.CDLL("./libkernel.so")
    except OSError as e:
        print(f"Error loading library: {e}")
        exit(1)

# this code Python version of the conversion 
def convert_RGB_to_YCbCr(rgb_pixels, width, height):
    ycc_pixels = bytearray(width * height * 3)
    # recall the pixels are in row major order
    # and that there are 3 bytes per pixel in r,g,b order 

    # Your code goes here.
    totalPixels = width*height*3
    for i in range(0,len(rgb_pixels), 3):
        r = rgb_pixels[i]
        g = rgb_pixels[i+1]
        b = rgb_pixels[i+2]

        y = int(round(0.299 * r + 0.587 * g + 0.114 * b))
        cb = int(round(128 - 0.168736 * r - 0.331264 * g + 0.5 * b))
        cr = int(round(128 + 0.5 * r - 0.418688 * g - 0.081312 * b))

        ycc_pixels[i] = max(0, min(255, y))
        ycc_pixels[i + 1] = max(0, min(255, cb))
        ycc_pixels[i + 2] = max(0, min(255, cr))

    #return totalPixels
        
    
    # do not use any libraries. 
    
    return ycc_pixels

def load_image_convert_to_RGB(image_path):
    img = cv2.imread(image_path)
    if img is None:
        raise FileNotFoundError(f"Image not found: {image_path}")
    img_rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    height, width, _ = img_rgb.shape
    img_bytes = img_rgb.tobytes()
    return (width, height, img_bytes)

def save_grayscale_image(y_pixels, width, height, output_path):
    y_array = np.array(y_pixels, dtype=np.uint8).reshape((height, width))
    img = Image.fromarray(y_array, mode="L")
    img.save(output_path, format="JPEG")
    # print(f"Black & White image saved to {output_path}")

from collections import Counter

def compare_bytearrays_within_range(array1, array2, error):
    if len(array1) != len(array2):
        print("Bytearrays have different lengths.")
        return (False, 0)

    # Convert arrays to int16 for proper subtraction
    arr1 = np.array(array1, dtype=np.int16)
    arr2 = np.array(array2, dtype=np.int16)

    # Compute absolute differences
    differences = np.abs(arr1 - arr2)
    diff_mask = differences > error
    bytes_different = np.sum(diff_mask)

    # Get indices where values differ
    diff_indices = np.where(diff_mask)[0]

    identical = bytes_different == 0

    if bytes_different > 0:
        print("\nFirst 10 differing values:")
        print(f"{'Index':<8} {'Array1':<8} {'Array2':<8} {'Diff':<8}")
        print("-" * 30)

        # Print the first 10 differing values
        for idx in diff_indices[:10]:
            print(f"{idx:<8} {arr1[idx]:<8} {arr2[idx]:<8} {differences[idx]:<8}")

    # Count occurrences of each difference value
    diff_counts = Counter(differences[diff_mask])
    for diff_value, count in sorted(diff_counts.items()):
        print(f"{diff_value:<12}{count:<10}")

    return (identical, bytes_different)

def main():
    parser = argparse.ArgumentParser(description="Convert JPEG to YCbCr using C and Python, compare outputs, and generate a grayscale image.")
    parser.add_argument("-i", "--input", required=True, help="Input JPEG file")
    parser.add_argument("-o", "--output", required=True, help="Output JPEG file (Black & White)")
    parser.add_argument("-e", "--error", type=int, default=1, help="Error range for byte comparison")
    args = parser.parse_args()
    
    width, height, RGB_pixels = load_image_convert_to_RGB(args.input)
    total_pixels = width * height   
    
    start_time_py = time.time()
    ycc_pixels_py = convert_RGB_to_YCbCr(RGB_pixels, width, height)
    end_time_py = time.time()
    time_taken_py = end_time_py - start_time_py

    # load the C kernel into the Python Program 
    my_lib = load_library()

    ycc_pixels_c = bytearray(width * height * 3)

    # create the Python objects to hold the byte arrays we give to the C kernel 
    rgb_ctypes = (ctypes.c_ubyte * len(RGB_pixels)).from_buffer_copy(RGB_pixels)
    ycc_ctypes = (ctypes.c_ubyte * len(ycc_pixels_c)).from_buffer(ycc_pixels_c)

    # convert using the C kernel 
    start_time_c = time.time()
    my_lib.convert_to_YCrCb(rgb_ctypes, ycc_ctypes, width, height)
    end_time_c = time.time()
    time_taken_c = end_time_c - start_time_c

    print(f"Converted {total_pixels} pixels with the C kernel in {time_taken_c} seconds and with the Python code in {time_taken_py} seconds.")
        
    identical, num_bytes_diff = compare_bytearrays_within_range(ycc_pixels_py, ycc_pixels_c, args.error)
    print(f"Similarity Check: {identical}, Different Bytes: {num_bytes_diff}")
    
    save_grayscale_image(ycc_ctypes[::3], width, height, args.output)

if __name__ == "__main__":
    main()
