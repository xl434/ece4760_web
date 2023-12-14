## **Project Introduction**

### One sentence "sound bite" that describes your project.
“Pi-Looper”, an innovative and affordable loop station empowers musicians to record and create their musical journey. 
### A summary of what you did and why.
* A loop station is a music device used by musicians for recording, looping, and playing back audio snippets in real-time during performance. It allows musicians to capture and reproduce short musical segments, often played on instruments or vocals, and then layer these recordings on top of each other to build complex arrangements. 
* We developed a loop station called “Pi-Looper” using Raspberry Pi Pico that empowers musicians to effortlessly layer, record, and save instrument tracks. Unlike traditional loop stations, “Pi-Looper” incorporates FRAM to enable users to save and retrieve tracks through serial communication, providing a new level of flexibility and persistence in music creations. We designed and tested an amplifier circuit to amplify instrument signal, in our case a guitar signal, to an optimal input range for the pico’s ADC. We then implemented loop station logic by designing a finite state machine (FSM) based on button inputs for record, playback, stop, and clear functionalities.

## **High level design**
### Rationale and sources of our project idea
* The inspiration of “Pi-Looper” emerged from a shared passion of music of all three members. We noticed that sometimes it’s very difficult for solo performers to create a full and dynamic sound without additional band members. With our experience in Lab1, where we synthesized bird songs using a DAC, we saw the potential to record, process, and playback audio using Raspberry Pi Pico. Therefore, we decided to build a loop station that allows solo performers to build up layers during a live performance and create intricate arrangements that appear as multiple musicians are playing simultaneously.
### background math
#### Non-inverting Amplifier Circuit
A non-inverting amplifier circuit is an operational amplifier (op-amp) configuration where the input signal is connected to the non-inverting terminal (+) and the feedback is applied to the inverting terminal (-). The output is taken from the opamp’s output terminal. The key characteristic of a non-inverting amplifier is that it provides a voltage gain, which means the output voltage is amplified in relation to the input voltage. 

