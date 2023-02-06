// code added to Android app (message handler section)

// [ . . . ]
else if ( message.contains("BLOCK ") ) {
    // a current sensor changed state
    String[] strings = message.split("\\s+");
    if ( strings.length > 3 ) {
	int block = Integer.parseInt(strings[1]);
	if ( strings[2].contains("OFF") )
	    layoutLayers.setDrawable(block, blocksOff.getDrawable(block));
	if ( strings[2].contains("STANDING") )
	    layoutLayers.setDrawable(block, blocksStanding.getDrawable(block));
	if ( strings[2].contains("RUNNING") )
	    layoutLayers.setDrawable(block, blocksRunning.getDrawable(block));
	
	layout.setImageDrawable(null);
	layout.setImageDrawable(layoutLayers);
    }
}
else if ( message.contains("TRAIN ") && message.contains("STATUS") ) {
    String[] strings = message.split("\\s+");
    if ( strings.length > 3 ) {
	int track = Integer.parseInt(strings[1]);
	if ( track == 1 )
	    direction1.setText(strings[3]);
	else
	    direction2.setText(strings[3]);
    }
}


// [ . . . ]
