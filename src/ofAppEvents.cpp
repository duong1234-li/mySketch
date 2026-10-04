#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	if (key == '1') {
		selectTool(Tool::Brush);
	} else if (key == '2') {
		selectTool(Tool::Pen);
	} else if (key == '3') {
		selectTool(Tool::Pencil);
	} else if (key == '4') {
		selectTool(Tool::Marker);
	} else if (key == '5') {
		selectTool(Tool::Paintbrush);
	} else if (key == '6' || key == 'f' || key == 'F') {
		selectTool(Tool::Fill);
	} else if (key == 'e' || key == 'E') {
		eraser = true;
	} else if ((key == 'z' || key == 'Z') && ofGetKeyPressed(OF_KEY_CONTROL)) {
		undoLastStroke();
	} else if (key == 'c' || key == 'C') {
		clearCanvas();
	} else if (key == 's' || key == 'S') {
		saveArtwork();
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){
	pollTabletPressure();
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
	pollTabletPressure();
	if (draggingLayerOpacity) {
		setActiveLayerOpacity(x);
		return;
	}
	if (!pendingStroke || !canvasBounds.inside(x, y)) {
		return;
	}
	const ofVec2f currentPoint(x - canvasBounds.x, y - canvasBounds.y);
	appendStrokePoint(currentPoint, effectivePressure());
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
	pollTabletPressure();
	if (handleLayerPanelPress(x, y) || gui.getShape().inside(x, y) ||
		!canvasBounds.inside(x, y)) {
		return;
	}
	const int localX = x - static_cast<int>(canvasBounds.x);
	const int localY = y - static_cast<int>(canvasBounds.y);
	if (selectedTool == Tool::Fill && !eraser) {
		fillAt(localX, localY);
		return;
	}
	drawingStroke = false;
	pendingStroke = true;
	pendingMotionSamples = 0;
	resetSimulatedPressure();
	activeStroke.points.clear();
	activeStroke.pressures.clear();
	activeStroke.fillSpans.clear();
	activeStroke.points.emplace_back(localX, localY);
	activeStroke.pressures.push_back(effectivePressure());
	pendingStrokeStartedAt = ofGetElapsedTimeMillis();
	activeStroke.color = brushColor;
	activeStroke.color.a = brushOpacity;
	activeStroke.tool = selectedTool == Tool::Fill ? Tool::Brush : selectedTool;
	activeStroke.width = brushSize;
	if (!eraser) {
		switch (selectedTool) {
			case Tool::Pen:
				activeStroke.width *= 0.4f;
				break;
			case Tool::Pencil:
				activeStroke.width *= 0.55f;
				activeStroke.color.a = static_cast<unsigned char>(activeStroke.color.a * 0.55f);
				break;
			case Tool::Marker:
				activeStroke.width *= 1.7f;
				activeStroke.color.a = static_cast<unsigned char>(activeStroke.color.a * 0.32f);
				break;
			case Tool::Paintbrush:
				activeStroke.width *= 1.35f;
				break;
			case Tool::Brush:
			case Tool::Fill:
				break;
		}
	}
	activeStroke.eraser = eraser;
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){
	if (draggingLayerOpacity) {
		draggingLayerOpacity = false;
		return;
	}
	pollTabletPressure();
	if (pendingStroke && drawingStroke) {
		if (canvasBounds.inside(x, y)) {
			const ofVec2f endPoint(x - canvasBounds.x, y - canvasBounds.y);
			if ((endPoint - activeStroke.points.back()).length() > 0.5f) {
				activeStroke.points.push_back(endPoint);
				activeStroke.pressures.push_back(effectivePressure());
			}
		}
		finishActiveStroke();
	} else if (pendingStroke) {
		pendingStroke = false;
		activeStroke.points.clear();
		activeStroke.pressures.clear();
	}
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){
	finishActiveStroke();
	pendingStroke = false;
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}
