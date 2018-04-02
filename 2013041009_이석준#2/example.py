import RPi.GPIO as GPIO
import random
import time
import sys

GPIO.setmode(GPIO.BCM)

TRIG = 23
ECHO = 24

GPIO.setup(16, GPIO.OUT)                #LED
GPIO.setup(20, GPIO.OUT)
GPIO.setup(21, GPIO.OUT)

GPIO.setup(13, GPIO.IN)                 #BUTTON
GPIO.setup(19, GPIO.IN)
GPIO.setup(26, GPIO.IN)

GPIO.setup(TRIG, GPIO.OUT)              #ULTRASONIC
GPIO.setup(ECHO, GPIO.IN)

score = 0
isReleased = True
#isReleased2 = True
#isReleased3 = True

def distance():
        GPIO.output(TRIG, True)
        time.sleep(0.00001)
        GPIO.output(TRIG, False)

        pulse_start = time.time()
        pulse_end = time.time()

        while GPIO.input(ECHO) == 0:
                pulse_start = time.time()

        while GPIO.input(ECHO) == 1:
                pulse_end = time.time()

        pulse_duration = pulse_end - pulse_start

        distance = pulse_duration * 17150
        distance = round(distance, 2)

        return distance

for n in range(5):
        distance()
        print "Distance : ", distance, "cm"
        if distance < 20:
                break
        else:
                if (n == 4):
                        print ("Please try again")
                        sys.exit(1)
        time.sleep(1)

GPIO.output(16, GPIO.HIGH)
GPIO.output(20, GPIO.HIGH)
GPIO.output(21, GPIO.HIGH)

print ("Let's play game")

def push(btn):
        global isReleased
        global score

        for i in range(3):
                if (btn[i][0] == True and isReleased == True):
                        GPIO.output(btn[i][1], GPIO.LOW)
                        score += 1
                        print ("My score is " + str(score))
                        isReleased = False
                        time.sleep(random.randrange(1, 3))
                        GPIO.output(btn[i][1], GPIO.HIGH)

                if (btn[i][0] == True and isReleased == False):
                        isReleased = True

                time.sleep(.01)

while True:
        btn = [
                [GPIO.input(13), 16],
                [GPIO.input(19), 20],
                [GPIO.input(26), 21]
        ]

        push(btn)

#       GPIO.output(16, GPIO.HIGH)
#       GPIO.output(20, GPIO.HIGH)
#       GPIO.output(21, GPIO.HIGH)
#
#       if (bt2 == True and isReleased2 == True):
#               GPIO.output(20, GPIO.LOW)
#               score += 1
#               print ("My score is " + str(score))
#               isReleased2 = False
#               time.sleep(random.randrange(1,3))
#               GPIO.output(20, GPIO.HIGH)
#
#       if (bt2 == False and isReleased2 == False):
#               isReleased2 = True
#
#               time.sleep(.01)
#
#       if (bt2 == True and isReleased2 == True):
#               GPIO.output(20, GPIO.LOW)
#               score += 1
#               print ("My score is " + str(score))
#               isReleased2 = False
#               time.sleep(random.randrange(1,3))
#               GPIO.output(20, GPIO.HIGH)
#
#       if (bt2 == False and isReleased2 == False):
#               isRelease2 = True
#
#               time.sleep(.01)
#
#       if (bt3 == True and isReleased3 == True):
#               GPIO.output(21, GPIO.LOW)
#               score += 1
#               print ("My score is " + str(score))
#               isReleased3 = False
#               time.sleep(random.randrange(1,3))
#               GPIO.output(21, GPIO.HIGH)
#
#       if (bt3 == False and isReleased3 == False):
#               isRelease3 = True

GPIO.cleanup()



