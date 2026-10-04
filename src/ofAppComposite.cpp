#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::renderStroke(const Stroke &stroke){
	applyStroke(activeLayer(), stroke);
	rebuildCanvas();
}

//--------------------------------------------------------------
void ofApp::applyStroke(Layer &layer, const Stroke &stroke){
	drawStrokeLayer(stroke);
	layer.image.begin();
	ofEnableAlphaBlending();
	if (stroke.eraser) {
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
		ofSetColor(255);
		strokeLayer.draw(0, 0);
	} else {
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		ofSetColor(stroke.color.a, stroke.color.a, stroke.color.a, stroke.color.a);
		strokeLayer.draw(0, 0);
	}
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	layer.image.end();
	ofEnableAlphaBlending();
}

//--------------------------------------------------------------
void ofApp::rebuildLayer(Layer &layer){
	layer.image.begin();
	ofClear(0, 0, 0, 0);
	layer.image.end();
	for (const auto &stroke : layer.strokes) {
		applyStroke(layer, stroke);
	}
}

//--------------------------------------------------------------
void ofApp::rebuildCanvas(){
	canvas.begin();
	ofClear(paperColor);
	canvas.end();
	canvas.begin();
	ofEnableAlphaBlending();
	for (const auto &layer : layers) {
		if (layer->visible) {
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			const int opacity = static_cast<int>(layer->opacity * 255.0f);
			ofSetColor(opacity, opacity, opacity, opacity);
			layer->image.draw(0, 0);
		}
	}
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	canvas.end();
}

//--------------------------------------------------------------
void ofApp::saveArtwork(){
	ofFbo::Settings settings;
	settings.width = static_cast<int>(canvasBounds.width);
	settings.height = static_cast<int>(canvasBounds.height);
	settings.internalformat = GL_RGBA;
	settings.useDepth = false;
	settings.useStencil = false;
	exportBuffer.allocate(settings);
	exportBuffer.begin();
	ofClear(paperColor);
	ofEnableAlphaBlending();
	canvas.draw(0, 0);
	exportBuffer.end();

	ofPixels pixels;
	exportBuffer.readToPixels(pixels);
	ofFileDialogResult result = ofSystemSaveDialog("Artwork.png", "Export canvas as PNG");
	if (result.bSuccess) {
		ofSaveImage(pixels, result.getPath());
	}
}
