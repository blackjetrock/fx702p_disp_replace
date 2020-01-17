#!/usr/bin/tclsh

set fn [lindex $argv 0]

set f [open $fn]
set txt [read $f ]
close $f

set ::MAP {
    0x0f " "
    0x11 "?"
    0x14 "\""
    0x18 ":"
    0x30 "0"
    0x31 "1"
    0x32 "2"
    0x33 "3"
    0x34 "4"
    0x35 "5"
    0x36 "6"
    0x37 "7"
    0x38 "8"
    0x39 "9"
    0x40 "A"
    0x41 "B"
    0x42 "C"
    0x43 "D"
    0x44 "E"
    0x45 "F"
    0x46 "G"
    0x47 "H"
    0x48 "I"
    0x49 "J"
    0x4A "K"
    0x4B "L"
    0x4C "M"
    0x4D "N"
    0x4E "O"
    0x4F "P"
    0x50 "Q"
    0x51 "R"
    0x52 "S"
    0x53 "T"
    0x54 "U"
    0x55 "V"
    0x56 "W"
    0x57 "X"
    0x58 "Y"
    0x59 "Z"
    0x60 "CNT"
    0x5B "SX"
    0x5C "SY"
    
}

proc fx702pchar {n} {
    foreach {nx cx} $::MAP {
	if { $n == $nx } {
	    return $cx
	}
	
    }
    return "???"
}

set str ""
set d1 0
set d2 0

foreach line [split $txt "\n"] {

    if { [regexp -- {[0-9]+[ ]+([0-9A-F]+)[ ]+([0-9A-F]+)[ ]+([0-9A-F]+)[ ]+([0-9A-F]+)[ ]+([0-9A-F]+)} $line all addr data oe we op] } {
	#puts "$addr $data $oe $we $op"

	if { $we == 0 } {
	    #puts "$addr <- $data"
	    set sp [string repeat " " [expr 1*0x$addr]]
	    puts "$sp$data"
	}
	
	
    }
}

