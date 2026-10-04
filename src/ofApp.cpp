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
	canvas.draw(canvasBounds.x, canvasBounds.y);

	if (drawingStroke) {
		ofPushMatrix();
		ofTranslate(canvasBounds.x, canvasBounds.y);
		drawStroke(activeStroke);
		ofPopMatrix();
	}

	gui.draw();
	drawLayerPanel();
	ofSetColor(220, 230, 232);
	ofDrawBitmapString("1 BRUSH  2 PEN  3 PENCIL  4 MARKER  5 PAINTBRUSH  6 FILL  E ERASER", 16, ofGetHeight() - 16);
}

//--------------------------------------------------------------
void ofApp::resizeCanvas(int width, int height){
	const float oldWidth = canvas.isAllocated() ? canvas.getWidth() : 0.0f;
	const float oldHeight = canvas.isAllocated() ? canvas.getHeight() : 0.0f;
	canvasBounds.set(286, 52, std::max(1, width - 310), std::max(1, height - 76));

	const float scaleX = oldWidth > 0 ? canvasBounds.width / oldWidth : 1.0f;
	const float scaleY = oldHeight > 0 ? canvasBounds.height / oldHeight : 1.0f;
	if (oldWidth > 0 && oldHeight > 0) {
		for (auto &layer : layers) {
			for (auto &stroke : layer->strokes) {
				for (auto &point : stroke.points) {
					point.x *= scaleX;
					point.y *= scaleY;
				}
				for (auto &span : stroke.fillSpans) {
					span.x = static_cast<int>(span.x * scaleX);
					span.y = static_cast<int>(span.y * scaleY);
					span.width = static_cast<int>(span.width * scaleX);
				}
				stroke.width *= (scaleX + scaleY) * 0.5f;
			}
		}
	}

	ofFbo::Settings settings;
	settings.width = static_cast<int>(canvasBounds.width);
	settings.height = static_cast<int>(canvasBounds.height);
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
