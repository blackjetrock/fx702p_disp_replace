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
    0x5E "_"
    
}

proc fx702pchar {n} {
    foreach {nx cx} $::MAP {
	if { $n == $nx } {
	    return $cx
	}
	
    }
    return [format "?%02X?" $n]
}

set ::A {C E}
set ::B {D F}

foreach a $::A b $::B {
    puts "a,b = $a, $b"
    for {set i 3} {$i <= 12} {incr i 1} {
	set hi [format "%02X" $i]
	set ::DATA($a,$hi) " "
	set ::DATA($b,$hi) " "
    }
}

    for {set i 1} {$i <= 12} {incr i 1} {
	set hi [format "%02X" $i]
	set ::ANN($hi) " "

    }

proc display_regs {a b} {
    puts -nonewline "<"
    foreach a $::A b $::B {
	for {set i 3} {$i <= 12} {incr i 1} {
	    set hi [format "%02X" [expr (9-($i-3))+3]]
	    
	    if { ($::DATA($a,$hi) == " ") || ($::DATA($b,$hi)== " ") } {
		set ::LCD($hi) " "
	    } else {
		
		
		set h $::DATA($a,$hi)
		set l $::DATA($b,$hi)
		
		set v $h$l
		set ch [fx702pchar [expr 0x$v]]
		set ::LCD($hi) $ch
	    }
	    puts -nonewline $::LCD($hi)
	}

    }
    puts ">"

    puts -nonewline "<"
    for {set i 1} {$i <= 12} {incr i 1} {
	set hi [format "%02X" [expr $i]]
	switch $::ANN($hi) {
	    0 {
		set a "    "
	    }
	    8 {
		switch $hi {
		    0B {
			set a " 0B "
		    }
		    0A {
			set a " DEG"
		    }
		    09 {
			set a " ARC"
		    }
		    08 {
			set a " HYP"
		    }
		    07 {
			set a " 07 "
		    }
		    06 {
			set a " TRC"
		    }
		    05 {
			set a " 05 "
		    }
		    04 {
			set a " PRT"
		    }
		    03 {
			set a " 03 "
		    }
		    02 {
			set a " 02 "
		    }
		    01 {
			set a " 01 "
		    }
		}
	    }
	    default {
		set a "    "
	    }
	}
	
	puts -nonewline $a
    }
    puts ">"
}

set str ""
set d1 0
set d2 0

set ra "-"
set an "-"

set ::DEBUG         0
set ::DEBUG_W       1
set ::DISPLAY_ON    0
set ::DELTA_DISPLAY 1
set ::CLOCK_PHASE   0

foreach line [split $txt "\n"] {

    if { [regexp -- {([0-9]+)[ ]+([0-9A-F]+)[ ]+([0-9A-F]+)[ ]+([0-9A-F]+)[ ]+([0-9A-F]+)[ ]+([0-9A-F]+)} $line all pos addr data oe we op] } {
	if { [string length $addr] == 1 } {
	    set addr "0$addr"
	}
	#puts "<<$line>>"
	#puts "pos=$pos A=$addr D=$data OE=$oe WE=$we OP=$op"

	if { $pos == 0000 } {
	    continue
	}
	
        if { [expr ([string trimleft $pos 0] % 2)] == $::CLOCK_PHASE } {
            continue
        }

	# Indicators
	if { ($oe == 1) && ($addr == "0B") && ($op == 0) } {
	    set an "$data"
	    puts "                                                 an=$an"
	} else {
	    #set ra "-"
	}

       # Characters
	if { ($oe == 1) && ($addr == 03) && ($op == 0) } {
	    set ra "$data"
	    set an "-"
	    puts "                                    ra=$ra"
	} else {
	    #set ra "-"
	}

	if { ($oe == 1) && ($an  == "4") && ($op == 1) } {
	    set ::ANN($addr) $data
	    puts "                                    ANN($addr)=$data"
	}
	
	if { ($oe == 1) && ($addr == "0A") && ($op == 0) } {
	    set ra "-"
	    puts "                                    ra=$ra"
	}

	if { ($oe == 1) && ($addr == "00") && ($op == 0) } {
	    set ra "-"
	    puts "                                    ra=$ra"
	}

	switch $op {
 	    0 {
		set opstr "CMD "
	    }
 	    1 {
		set opstr "DATA"
	    }
	    default {
		set opstr "????"
	    }
	    
	}
	
	if { $we == 0 } {
	    if { $::DEBUG_W } {
		puts "$opstr $addr <- $data"
	    }
	    set sp [string repeat " " [expr 1*0x$addr]]
	    if { $::DEBUG } {
		puts "$pos $addr $ra $oe $op $we  $sp$data"
	    }
	    set ::DATA($ra,$addr) $data
	} else {
	    if { $::DEBUG_W } {
		puts "$opstr $addr -> $data"
	    }
	    set sp [string repeat " " [expr 32+1*0x$addr]]
	    if { $::DEBUG } {
		puts "$pos $addr $ra $oe $op $we  $sp$data"
	    }
        }

	if { $::DISPLAY_ON } {
	    display_regs $::A $::B
	}

    }
}


