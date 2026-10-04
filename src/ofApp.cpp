#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	ofSetFrameRate(60);
	ofSetVerticalSync(true);
	ofSetBackgroundAuto(true);
	ofEnableAlphaBlending();
	ofSetLineWidth(1.2f);

	backgroundColor = ofColor(8, 12, 18);
	paperColor = ofColor(248, 246, 238);
	resizeCanvas(ofGetWidth(), ofGetHeight());

	gui.setup("CANVAS TOOLS");
	gui.setPosition(16, 16);
	gui.setHeaderBackgroundColor(ofColor(28, 130, 142));
	gui.setBackgroundColor(ofColor(8, 15, 20, 230));
	gui.setBorderColor(ofColor(55, 100, 108));
	gui.setTextColor(ofColor(220, 235, 232));
	gui.setFillColor(ofColor(39, 164, 157));
	gui.add(brushColor.set("Brush color", ofColor(228, 83, 71), ofColor(0), ofColor(255)));
	gui.add(brushTool.set("Brush", true));
	gui.add(penTool.set("Pen", false));
	gui.add(pencilTool.set("Pencil", false));
	gui.add(markerTool.set("Marker", false));
	gui.add(paintbrushTool.set("Paintbrush", false));
	gui.add(fillTool.set("Fill bucket", false));
	gui.add(moveResizeTool.set("Move/Resize", false));
	gui.add(brushSize.set("Brush size", 12.0f, 1.0f, 80.0f));
	gui.add(brushOpacity.set("Opacity", 255, 10, 255));
	gui.add(pressureSensitivity.set("Pressure", 1.0f, 0.0f, 1.0f));
	gui.add(pressureSimulation.set("Sim pressure", 1.0f, 0.0f, 1.0f));
	gui.add(fillTolerance.set("Fill tolerance", 0, 0, 32));
	gui.add(eraser.set("Eraser", false));
	gui.add(undoAction.set("Undo stroke"));
	gui.add(clearAction.set("Clear active layer"));
	gui.add(saveAction.set("Export PNG"));
	undoAction.addListener(this, &ofApp::undoLastStroke);
	clearAction.addListener(this, &ofApp::clearCanvas);
	saveAction.addListener(this, &ofApp::saveArtwork);
	brushTool.addListener(this, &ofApp::brushToolChanged);
	penTool.addListener(this, &ofApp::penToolChanged);
	pencilTool.addListener(this, &ofApp::pencilToolChanged);
	markerTool.addListener(this, &ofApp::markerToolChanged);
	paintbrushTool.addListener(this, &ofApp::paintbrushToolChanged);
	fillTool.addListener(this, &ofApp::fillToolChanged);
	moveResizeTool.addListener(this, &ofApp::moveResizeToolChanged);
	addLayer();
	setupTabletPressureInput();
	resetSimulatedPressure();
}

//--------------------------------------------------------------
void ofApp::update(){
	pollTabletPressure();
}

//--------------------------------------------------------------
void ofApp::draw(){
	ofBackground(backgroundColor);
	ofSetColor(0, 0, 0, 70);
	ofDrawRectangle(canvasBounds.x + 5, canvasBounds.y + 5,
		canvasBounds.width, canvasBounds.height);
	ofSetColor(255);
	canvas.draw(canvasBounds.x, canvasBounds.y, canvasBounds.width, canvasBounds.height);
	if (selectedTool == Tool::MoveResize) {
		const Layer &layer = activeLayer();
		const ofRectangle content = selectedContentBounds(layer);
		if (content.width > 0.0f && content.height > 0.0f) {
			const ofRectangle bounds(
				canvasBounds.x + (layer.transformPosition.x + content.x * layer.transformScale) * canvasViewScale,
				canvasBounds.y + (layer.transformPosition.y + content.y * layer.transformScale) * canvasViewScale,
				content.width * layer.transformScale * canvasViewScale,
				content.height * layer.transformScale * canvasViewScale);
			ofPushStyle();
			ofNoFill();
			ofSetColor(30, 205, 190);
			ofSetLineWidth(2.0f);
			ofDrawRectangle(bounds);
			ofFill();
			const float handleSize = 12.0f;
			const ofVec2f handles[] = {
				ofVec2f(bounds.x, bounds.y),
				ofVec2f(bounds.x + bounds.width, bounds.y),
				ofVec2f(bounds.x + bounds.width, bounds.y + bounds.height),
				ofVec2f(bounds.x, bounds.y + bounds.height)
			};
			for (const auto &handle : handles) {
				ofSetColor(8, 15, 20);
				ofDrawRectangle(handle.x - handleSize * 0.5f - 1,
					handle.y - handleSize * 0.5f - 1, handleSize + 2, handleSize + 2);
				ofSetColor(220, 250, 245);
				ofDrawRectangle(handle.x - handleSize * 0.5f,
					handle.y - handleSize * 0.5f, handleSize, handleSize);
			}
			ofPopStyle();
		}
	}

	if (drawingStroke) {
		const Layer &layer = activeLayer();
		ofPushMatrix();
		ofTranslate(canvasBounds.x + layer.transformPosition.x * canvasViewScale,
			canvasBounds.y + layer.transformPosition.y * canvasViewScale);
		ofScale(canvasViewScale * layer.transformScale, canvasViewScale * layer.transformScale);
		drawStroke(activeStroke);
		ofPopMatrix();
	}

	gui.draw();
	drawLayerPanel();
	ofSetColor(220, 230, 232);
	ofDrawBitmapString("1 BRUSH  2 PEN  3 PENCIL  4 MARKER  5 PAINTBRUSH  6 FILL  7 MOVE/RESIZE  E ERASER  I IMG  D DEL IMG  R RESET IMG", 16, ofGetHeight() - 16);
}

//--------------------------------------------------------------
void ofApp::resizeCanvas(int width, int height){
	const float availableWidth = std::max(1.0f, static_cast<float>(width - 310));
	const float availableHeight = std::max(1.0f, static_cast<float>(height - 76));
	canvasViewScale = std::min(availableWidth / canvasWidth, availableHeight / canvasHeight);
	const float displayWidth = canvasWidth * canvasViewScale;
	const float displayHeight = canvasHeight * canvasViewScale;
	canvasBounds.set(286.0f + (availableWidth - displayWidth) * 0.5f,
		52.0f + (availableHeight - displayHeight) * 0.5f, displayWidth, displayHeight);

	ofFbo::Settings settings;
	settings.width = canvasWidth;
	settings.height = canvasHeight;
	settings.internalformat = GL_RGBA;
	settings.useDepth = false;
	settings.useStencil = false;
	canvas.allocate(settings);
	strokeLayer.allocate(settings);
	exportBuffer.allocate(settings);
	detectFillPixelOrientation();
	for (auto &layer : layers) {
		layer->image.allocate(settings);
		rebuildLayer(*layer);
	}
	rebuildCanvas();
	updateLayerPanelLayout();
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){
	resizeCanvas(w, h);
}
