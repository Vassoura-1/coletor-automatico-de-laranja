import cv2
import sys
import numpy as np
import traceback

# Creating a VideoCapture object to read the video
cap = cv2.VideoCapture('video-limao.mp4')
 
try:
    # Loop until the end of the video
    while (cap.isOpened()):
    
        # Capture frame-by-frame
        ret, frame = cap.read()
        frame = cv2.resize(frame, (352,640), fx = 0, fy = 0,
                            interpolation = cv2.INTER_CUBIC)

        # frame = frame[:200,:200]
    
    
        #split channels for operations
        b,g,r = cv2.split(frame)
        S = b.astype(np.uint16)+g.astype(np.uint16)+r.astype(np.uint16) + 10e-5

        #making intrinsic space r and g coordinates
        r_in = (r>0)*(r/S)*255
        g_in = (g>0)*(g/S)*255

        #treasholding
        r_in = r_in.astype(np.uint8)
        g_in = g_in.astype(np.uint8)

        r_in_mean = int(r_in.mean())
        Tr = r_in_mean + (1.3*(r_in_mean>90)+1*(r_in_mean<=90))*r_in.std()
        g_in_mean = int(g_in.mean())
        Tg = r_in_mean + (1.3*(g_in_mean>90)+1*(g_in_mean<=90))*g_in.std()

        # # conversion of BGR to grayscale is necessary to apply this operation
        # gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    
        # adaptive thresholding to use different threshold
        # values on different regions of the frame.
        # Thresh1 = cv2.adaptiveThreshold(r_in, 255, cv2.ADAPTIVE_THRESH_MEAN_C,
        #                                     cv2.THRESH_BINARY_INV, 11, 2)
        # Thresh2 = cv2.adaptiveThreshold(g_in, 255, cv2.ADAPTIVE_THRESH_MEAN_C,
        #                                     cv2.THRESH_BINARY_INV, 11, 2)

        # _,Thresh1 = cv2.threshold(r_in, int(Tr), 255, cv2.THRESH_BINARY)
        # _,Thresh2 = cv2.threshold(g_in, int(Tg), 255, cv2.THRESH_BINARY)

        Thresh1 = 255*(r_in>=Tr)
        Thresh2 = 255*(g_in>=Tg)
    
        # Display the resulting frame
        cv2.imshow('Frame', frame)
        cv2.imshow('r_in', r_in)
        cv2.imshow('g_in', g_in)
        cv2.imshow('Thresh1', Thresh1.astype(np.uint8))
        cv2.imshow('Thresh2', Thresh2.astype(np.uint8))
        # define q as the exit button
        if cv2.waitKey(25) & 0xFF == ord('q'):
            break
except Exception as e: 
    print(e)
    print(traceback.format_exc())
# release the video capture object
cap.release()
# Closes all the windows currently opened.
cv2.destroyAllWindows()