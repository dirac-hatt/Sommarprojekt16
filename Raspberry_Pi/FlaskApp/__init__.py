from flask import Flask, render_template, request, redirect, url_for, session, flash, Response
from dbconnect import connect
import gc   # Garbage collection
from datetime import date
import numbers
from flask_socketio import SocketIO, emit
import serial

from picamera.array import PiRGBArray
from picamera import PiCamera
import time
import cv2
import numpy as np
import time

from threading import Thread

IR_0 = 0
IR_1 = 0
IR_2 = 0
IR_3 = 0
IR_4 = 0
IR_Yaw_right = 0
IR_Yaw_left = 0
Yaw = 0
p_part = 0
alpha = 0
Kp = 0
Kd = 0
AUTO_STATE = 0
manual_state = 0
mode = 0

latest_camera_frame = []

# dictionary for converting serial incoming data regarding the current manual state:
manual_states = {
    0: "Stop",
    1: "Forward",
    2: "Backward",
    3: "Right",
    4: "Left"
}

# dictionary for converting serial incoming data regarding the current auto state:
auto_states = {
    1: "DEAD_END",
    2: "CORRIDOR",
    3: "OUT_OF_CORRIDOR",
    4: "INTO_JUNCTION",
    5: "DETERMINE_JUNCTION",
    6: "JUNCTION_A_R",
    7: "JUNCTION_A_L",
    8: "JUNCTION_B_R",
    9: "JUNCTION_B_L",
    10: "JUNCTION_C",
    11: "JUNCTION_D",
    12: "OUT_OF_JUNCTION",
    13: 13,
    14: 14,
    15: 15,
    16: 16,
    17: 17,
    18: 18,
    19: 19,
    20: 20
}

# dictionary for converting serial incoming data regarding the current mode:
mode_states = {
    5: "Manual",
    6: "Auto"
}

# stop = 0
# forward = 1
# backward = 2
# right = 3
# left = 4
# manual = 5
# auto = 6
# Kp and kd = 7
# Only Kp = 8
# Only Kd = 9

# 9600 is the baudrate, should match serial baudrate in arduino
serial_port = serial.Serial("/dev/ttyACM0", 9600) 

app = Flask(__name__)
socketio = SocketIO(app, async_mode = "threading") # Without "async_mode = "threading", sending stuff to the cliend (via socketio) doesn't work!

def check_parameter_input(value):    
    if value:
        try:
            value = int(value)
        except:
            value = "" # value must be an integer! (so don't send the parameter)
            return value
        else: # if conversion was successful:
            if value < 0:
                value = "" # value must be positive! (so don't send the parameter)
                return value
            else: # if positive integer
                return value  
            
    else: # if value == 0 or value is empty (if the parameter field was left empty)
        return value
        
def video_thread():
    global latest_camera_frame
    
    camera = PiCamera()
    camera.hflip = True # | Rotate 180 deg if mounted upside down
    camera.vflip = True # |
    camera.resolution = (640, 480)
    camera.framerate = 32
    rawCapture = PiRGBArray(camera, size=(640, 480))
    video_stream = camera.capture_continuous(rawCapture, format="bgr", use_video_port=True)
    # allow the camera to warmup:
    time.sleep(0.1)
    
    # capture frames from the camera
    while 1:
        # grab the raw numpy array representing the image:
        latest_camera_frame = next(video_stream).array 
        # clear the stream in preparation for the next frame:
        rawCapture.truncate(0)        
        # Delay 0.05 sec (~ 20 Hz):
        time.sleep(0.05) 

def web_thread():
    while 1:
        socketio.emit("new_data", {"IR_0": IR_0, "IR_1": IR_1, "IR_2": IR_2, "IR_3": IR_3, "IR_4": IR_4, "IR_Yaw_right": IR_Yaw_right, "IR_Yaw_left": IR_Yaw_left, "Yaw": Yaw, "p_part": p_part, "alpha": alpha, "Kp": Kp, "Kd": Kd, "AUTO_STATE": AUTO_STATE, "manual_state": manual_state, "mode": mode})
        time.sleep(0.1) # Delay 0.1 sec (~ 0 Hz)
        
def serial_thread():
    # all global variables this function can modify:
    global IR_0, IR_1, IR_2, IR_3, IR_4, IR_Yaw_right, IR_Yaw_left, Yaw, p_part, alpha, Kp, Kd, AUTO_STATE, manual_state, mode

    while 1:
        no_bytes_waiting = serial_port.inWaiting()
        if no_bytes_waiting > 19: # the ardu sends 19 bytes at the time (17 data, 2 control)
            # read the first byte (read 1 byte): (ord: gives the actual value of the byte)
            first_byte = ord(serial_port.read(size = 1)) 
            
            # read all data bytes if first byte was the start byte:
            if first_byte == 100:
                serial_data = []
                # read all data bytes:
                for counter in range(17): # 17 data bytes is sent from the ardu
                    serial_data.append(ord(serial_port.read(size = 1)))
                # read last byte:
                last_byte = ord(serial_port.read(size = 1)) 
                
                # update the variables with the read serial data only if the last byte was the end byte:
                if last_byte == 200:
                    IR_0 = serial_data[0]
                    IR_1 = serial_data[1]
                    IR_2 = serial_data[2]
                    IR_3 = serial_data[3]
                    IR_4 = serial_data[4]
                    IR_Yaw_right = int(np.int8(serial_data[5])) # IR_Yaw_right was sent as an int8_t, but in order to make python treat it as one we first need to tell it so explicitly with the help of numpy, before converting the result (a number between -128 and +127) to the corresponding python int 
                    IR_Yaw_left = int(np.int8(serial_data[6]))
                    Yaw = int(np.int8(serial_data[7]))
                    p_part = int(np.int8(serial_data[8]))
                    alpha_low_byte = int(np.int8(serial_data[9]))
                    alpha_high_byte = int(np.int8(serial_data[10])) # yes, we need to first treat both the low and high bytes as int8:s, try it by hand and a simple example
                    alpha = alpha_low_byte + alpha_high_byte*256 # (mult with 256 corresponds to a 8 bit left shift)
                    Kp = serial_data[11]
                    Kd_low_byte = serial_data[12]
                    Kd_high_byte = serial_data[13]
                    Kd = Kd_low_byte + Kd_high_byte*256
                    AUTO_STATE = auto_states[serial_data[14]] # look up the received integer in the auto_states dict
                    manual_state = manual_states[serial_data[15]]
                    mode = mode_states[serial_data[16]]
                else: # if final byte doesn't match: something weird has happened during transmission: flush input buffer and start over
                    serial_port.flushInput()
                    print("Something went wrong in the transaction: final byte didn't match!")                  
            else: # if first byte isn't the start byte: we're not in sync: just read the next byte until we get in sync (until we reach the start byte)
                pass
        else: # if not enough bytes for entire transmission, just wait for more data:
            pass

        time.sleep(0.025) # Delay for ~40 Hz loop frequency (faster than the sending frequency)
    
def gen_normal():
    while 1:
        if len(latest_camera_frame) > 0:
            ret, jpg = cv2.imencode(".jpg", latest_camera_frame)
            frame = jpg.tobytes()
            yield (b'--frame\r\n'
                    b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')
                
def gen_mask():
    while 1:
        if len(latest_camera_frame) > 0:
            hsv = cv2.cvtColor(latest_camera_frame, cv2.COLOR_BGR2HSV)
            lower_red = np.array([30, 150, 50])
            upper_red = np.array([255, 255, 180])
            range_mask = cv2.inRange(hsv, lower_red, upper_red)
            
            ret, jpg = cv2.imencode(".jpg", range_mask)
            frame = jpg.tobytes()
            yield (b'--frame\r\n'
                    b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')
                
@app.route("/camera_normal")
def camera_normal():
    return Response(gen_normal(), mimetype='multipart/x-mixed-replace; boundary=frame')
    
@app.route("/camera_mask")
def camera_mask():
    return Response(gen_mask(), mimetype='multipart/x-mixed-replace; boundary=frame')
    
@app.route("/")   
@app.route("/index")
def index():
    try:
        thread_video = Thread(target = video_thread)
        thread_video.start()
        thread_web = Thread(target = web_thread)
        thread_web.start()
        thread_serial = Thread(target = serial_thread)
        thread_serial.start()
        return render_template("index.html") 
    except Exception as e:
        return render_template("500.html", error = str(e))
          
@app.route("/phone")
def phone():
    try:
        thread_video = Thread(target = video_thread)
        thread_video.start()
        thread_web = Thread(target = web_thread)
        thread_web.start()
        thread_serial = Thread(target = serial_thread)
        thread_serial.start()
        return render_template("phone.html") 
    except Exception as e:
        return render_template("500.html", error = str(e))        
        
@socketio.on("my event")
def handle_my_custom_event(sent_dict):
    print("Recieved message: " + sent_dict["data"])
       
@socketio.on("button_event")
def handle_button_event(sent_dict):
    print("Recieved message: " + sent_dict["data"])
    serial_port.write(sent_dict["data"] + "\n") # "\n" is used as a delimiter char when the arduino reads the serial port


@socketio.on("touch_event")
def handle_touch_event(sent_dict):
    print("Recieved message: " + sent_dict["data"])
    serial_port.write(sent_dict["data"] + "\n")

@socketio.on("key_event")
def handle_key_event(sent_dict):
    print("Recieved message: " + sent_dict["data"])
    serial_port.write(sent_dict["data"] + "\n")
    
@socketio.on("parameters_event")
def handle_parameters_event(sent_dict):
    Kp_input = sent_dict["Kp"]
    Kd_input = sent_dict["Kd"]
    
    print("Recieved Kp: " + Kp_input)
    print("Recieved Kd: " + Kd_input)
    
    Kp_input = check_parameter_input(Kp_input) # After this, Kp_input is non-empty only if the user typed a positive integer into the Kp field (and thus we should send it to the arduino)
    Kd_input = check_parameter_input(Kd_input)
        
    if (Kp_input or Kp_input == 0) and (Kd_input or Kd_input == 0): # if valid, non-empty input for both Kp and Kd: send both Kp and Kd
        serial_port.write("7" + "\n" + str(Kp_input) + "\n" + str(Kd_input) + "\n")
        print("New Kp and Kd sent!")

    elif Kp_input or Kp_input == 0: # if only Kp:
        serial_port.write("8" + "\n" + str(Kp_input) + "\n") # 8 = Just Kp
        print("New Kp sent!")

    elif Kd_input or Kd_input == 0: # if only Kd:
        serial_port.write("9" + "\n" + str(Kd_input) + "\n") # 9 = Just Kd
        print("New Kd sent!")
        
@app.errorhandler(404)
def page_not_found(e):
    try:
        return render_template("404.html") 
    except Exception as e:
        return render_template("500.html", error = str(e))
  
if __name__ == '__main__':
    socketio.run(app, "169.254.74.215", 80)