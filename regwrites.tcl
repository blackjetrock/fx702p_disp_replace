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
}

set str ""
set d1 0
set d2 0

set ra "-"

foreach line [split $txt "\n"] {

    if { [regexp -- {([0-9]+)[ ]+([0-9A-F]+)[ ]+([0-9A-F]+)[ ]+([0-9A-F]+)[ ]+([0-9A-F]+)[ ]+([0-9A-F]+)} $line all pos addr data oe we op] } {
	#puts "$addr $data $oe $we $op"

	if { $pos == 0000 } {
	    continue
	}
	
        if { [expr ([string trimleft $pos 0] % 2)] == 0 } {
            continue
        }

	
	if { ($oe == 1) && ($addr == 03) && ($op == 0) } {
	    set ra "$data"
	    #puts "                                    ra=$ra"
	} else {
	    #set ra "-"
	}

	if { ($oe == 1) && ($addr == "0A") && ($op == 0) } {
	    set ra "-"
	    #puts "                                    ra=$ra"
	}

	if { ($oe == 1) && ($addr == "00") && ($op == 0) } {
	    set ra "-"
	    #puts "                                    ra=$ra"
	}
	
	if { $we == 0 } {
	    #puts "$addr <- $data"
	    set sp [string repeat " " [expr 1*0x$addr]]
	    #puts "$pos $addr $ra $oe $op   $sp$data"
	    set ::DATA($ra,$addr) $data
	} else {
	    #puts "$addr <- $data"
	    set sp [string repeat " " [expr 32+1*0x$addr]]
	    #puts "$pos $addr $ra $oe $op   $sp$data"
        }

	display_regs $::A $::B

    }
}


