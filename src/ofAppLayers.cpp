#include "ofApp.h"

//--------------------------------------------------------------
ofApp::Layer &ofApp::activeLayer(){
	return *layers[activeLayerIndex];
}

//--------------------------------------------------------------
void ofApp::addLayer(){
	auto layer = std::make_unique<Layer>();
	layer->name = "Layer " + ofToString(++nextLayerNumber);
	ofFbo::Settings settings;
	settings.width = static_cast<int>(canvasBounds.width);
	settings.height = static_cast<int>(canvasBounds.height);
	settings.internalformat = GL_RGBA;
	settings.useDepth = false;
	settings.useStencil = false;
	layer->image.allocate(settings);
	layer->image.begin();
	ofClear(0, 0, 0, 0);
	layer->image.end();
	layers.push_back(std::move(layer));
	activeLayerIndex = layers.size() - 1;
	rebuildCanvas();
	updateLayerPanelLayout();
}

//--------------------------------------------------------------
void ofApp::removeActiveLayer(){
	if (layers.size() <= 1) {
		clearCanvas();
		return;
	}
	layers.erase(layers.begin() + activeLayerIndex);
	activeLayerIndex = std::min(activeLayerIndex, layers.size() - 1);
	rebuildCanvas();
	updateLayerPanelLayout();
}

//--------------------------------------------------------------
void ofApp::moveActiveLayer(int direction){
	const int destination = static_cast<int>(activeLayerIndex) + direction;
	if (destination < 0 || destination >= static_cast<int>(layers.size())) {
		return;
	}
	std::swap(layers[activeLayerIndex], layers[destination]);
	activeLayerIndex = static_cast<std::size_t>(destination);
	rebuildCanvas();
	updateLayerPanelLayout();
}

//--------------------------------------------------------------
void ofApp::undoLastStroke(){
	Layer &layer = activeLayer();
	if (!layer.strokes.empty()) {
		layer.strokes.pop_back();
		rebuildLayer(layer);
		rebuildCanvas();
	}
}

//--------------------------------------------------------------
void ofApp::clearCanvas(){
	activeLayer().strokes.clear();
	drawingStroke = false;
	pendingStroke = false;
	tabletPressureHeld = false;
	activeStroke.points.clear();
	activeStroke.pressures.clear();
	rebuildLayer(activeLayer());
	rebuildCanvas();
}
