# fx702p_disp_replace
Cla4800_pcb/proc2_cursor_123456.txt~asio fx702p display replacement

This is a replacement fx702p display that uses a 'bluepill' board to sniff the fx702p bus looking for display controller accesses. Those accesss are decoded and the display data that is found is written to a 4x20 character LCD display.

You have to connect several wires to pins inside the fx702p, I have a PCB for debug (which is a bit large). Some soldering is a bit fiddly, and there's quite a lot of connections. I used a PCb for debug, it's a bit large but works.

The sketch can operate in two modes:

ISR mode
This processes display controller accesses in the ISR routine, the main loop just displays the data on the LCD. This gives a good reponse time on the display.

Logic Analyser mode
This uses the ISR to capture the display access data and stores it in a buffer like a logic analyser does.
The main loop then decodes the logic analyser trace data to create the structures that allow the main loop to display the data on the LCD. This mode allows the trace information to be dumped to the serial port for debug. Unfortunately this mode can cause ISR overruns and it doesn't have a very good response time on the display. It's useful for debug though.

Problems
There are a few problems with the code:

1. The cursor doesn't work on the left half of the display. The right half is fine, the protocol is completely different on the two halves.
2. f1,f2,hyp and ARC are not properly decoded, and in ISR mode at the moment they aren't decoded at all.
3. The seven segment steps display is on all the time, but this could be viewed as an advantage over the original.
4. Some characters aren't displayed properly. I use the user defined characters for some, but my rendering of the characters is a bit iffy. I need to have a look at a working fx702p to see what dot patterns they used for some of these characters.



