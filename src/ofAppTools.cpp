#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::selectTool(Tool tool){
	selectedTool = tool;
	brushTool = tool == Tool::Brush;
	penTool = tool == Tool::Pen;
	pencilTool = tool == Tool::Pencil;
	markerTool = tool == Tool::Marker;
	paintbrushTool = tool == Tool::Paintbrush;
	fillTool = tool == Tool::Fill;
	eraser = false;
}

//--------------------------------------------------------------
void ofApp::brushToolChanged(bool &enabled){
	if (enabled) selectTool(Tool::Brush);
	else if (selectedTool == Tool::Brush) brushTool = true;
}

//--------------------------------------------------------------
void ofApp::penToolChanged(bool &enabled){
	if (enabled) selectTool(Tool::Pen);
	else if (selectedTool == Tool::Pen) penTool = true;
}

//--------------------------------------------------------------
void ofApp::pencilToolChanged(bool &enabled){
	if (enabled) selectTool(Tool::Pencil);
	else if (selectedTool == Tool::Pencil) pencilTool = true;
}

//--------------------------------------------------------------
void ofApp::markerToolChanged(bool &enabled){
	if (enabled) selectTool(Tool::Marker);
	else if (selectedTool == Tool::Marker) markerTool = true;
}

//--------------------------------------------------------------
void ofApp::paintbrushToolChanged(bool &enabled){
	if (enabled) selectTool(Tool::Paintbrush);
	else if (selectedTool == Tool::Paintbrush) paintbrushTool = true;
}

//--------------------------------------------------------------
void ofApp::fillToolChanged(bool &enabled){
	if (enabled) selectTool(Tool::Fill);
	else if (selectedTool == Tool::Fill) fillTool = true;
}
