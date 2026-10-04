#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::appendStrokePoint(const ofVec2f &point, float pressure){
	if (!pendingStroke) {
		return;
	}
	const float segmentLength = (point - activeStroke.points.back()).length();
	if (segmentLength <= 0.5f) {
		return;
	}
	activeStroke.points.emplace_back(point);
	activeStroke.pressures.push_back(pressure);
	if (drawingStroke) {
		return;
	}
	++pendingMotionSamples;
	const uint64_t heldFor = ofGetElapsedTimeMillis() - pendingStrokeStartedAt;
	const float distanceFromPress = (point - activeStroke.points.front()).length();
	if (pendingMotionSamples >= 3 && distanceFromPress >= 12.0f && heldFor >= 50) {
		drawingStroke = true;
	}
}

//--------------------------------------------------------------

void ofApp::drawStroke(const Stroke &stroke){
	const ofColor color = stroke.eraser ? ofColor(255) : stroke.color;
	ofPushStyle();
	ofSetColor(color);
	if (stroke.tool == Tool::Fill) {
		for (const auto &span : stroke.fillSpans) {
			ofDrawRectangle(span.x, span.y, span.width, 1);
		}
		ofPopStyle();
		return;
	}
	auto pressureAt = [&stroke](std::size_t index){
		const float pressure = index < stroke.pressures.size() ? stroke.pressures[index] : 1.0f;
		return std::sqrt(ofClamp(pressure, 0.0f, 1.0f));
	};
	if (stroke.points.size() == 1) {
		ofDrawCircle(stroke.points.front(), stroke.width * pressureAt(0) * 0.5f);
		ofPopStyle();
		return;
	}

	if (stroke.tool == Tool::Paintbrush && !stroke.eraser) {
		for (std::size_t i = 1; i < stroke.points.size(); ++i) {
			const ofVec2f start = stroke.points[i - 1];
			const ofVec2f end = stroke.points[i];
			const float pressure = (pressureAt(i - 1) + pressureAt(i)) * 0.5f;
			const float segmentWidth = stroke.width * pressure;
			ofSetLineWidth(std::max(0.6f, segmentWidth * 0.22f));
			ofVec2f normal(-(end.y - start.y), end.x - start.x);
			if (normal.length() > 0.001f) {
				normal.normalize();
			}
			for (int bristle = -2; bristle <= 2; ++bristle) {
				ofVec2f offset = normal * (segmentWidth * 0.16f * bristle);
				ofDrawLine(start + offset, end + offset);
			}
		}
	} else if (stroke.tool == Tool::Pencil && !stroke.eraser) {
		for (std::size_t i = 1; i < stroke.points.size(); ++i) {
			const ofVec2f start = stroke.points[i - 1];
			const ofVec2f end = stroke.points[i];
			const float pressure = (pressureAt(i - 1) + pressureAt(i)) * 0.5f;
			ofSetLineWidth(std::max(0.6f, stroke.width * pressure * 0.7f));
			ofDrawLine(start, end);
			ofVec2f normal(-(end.y - start.y), end.x - start.x);
			if (normal.length() > 0.001f) {
				normal.normalize();
			}
			ofColor grainColor = color;
			grainColor.a = static_cast<unsigned char>(color.a * 0.25f);
			ofSetColor(grainColor);
			ofDrawLine(start + normal * 0.6f, end + normal * 0.6f);
			ofSetColor(color);
		}
	} else {
		std::vector<ofVec2f> smoothPoints = stroke.points;
		for (std::size_t i = 1; i + 1 < stroke.points.size(); ++i) {
			smoothPoints[i] = (stroke.points[i - 1] + stroke.points[i] * 2.0f +
				stroke.points[i + 1]) * 0.25f;
		}

		ofMesh mesh;
		mesh.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
		for (std::size_t i = 0; i < smoothPoints.size(); ++i) {
			const ofVec2f previous = smoothPoints[i == 0 ? i : i - 1];
			const ofVec2f next = smoothPoints[i + 1 < smoothPoints.size() ? i + 1 : i];
			ofVec2f tangent = next - previous;
			if (tangent.length() <= 0.001f) {
				tangent.set(1.0f, 0.0f);
			} else {
				tangent.normalize();
			}
			ofVec2f normal(-tangent.y, tangent.x);
			const float halfWidth = stroke.width * pressureAt(i) * 0.5f;
			const ofVec2f left = smoothPoints[i] + normal * halfWidth;
			const ofVec2f right = smoothPoints[i] - normal * halfWidth;
			mesh.addVertex(ofVec3f(left.x, left.y, 0.0f));
			mesh.addColor(color);
			mesh.addVertex(ofVec3f(right.x, right.y, 0.0f));
			mesh.addColor(color);
		}
		mesh.draw();
		ofDrawCircle(smoothPoints.front(), stroke.width * pressureAt(0) * 0.5f);
		ofDrawCircle(smoothPoints.back(), stroke.width * pressureAt(stroke.points.size() - 1) * 0.5f);
	}
	ofPopStyle();
}

//--------------------------------------------------------------

void ofApp::drawStrokeLayer(const Stroke &stroke){
	Stroke opaqueStroke = stroke;
	if (!opaqueStroke.eraser) {
		opaqueStroke.color.a = 255;
	}
	strokeLayer.begin();
	ofClear(0, 0, 0, 0);
	ofPushStyle();
	ofEnableAlphaBlending();
	drawStroke(opaqueStroke);
	ofPopStyle();
	strokeLayer.end();
}

//--------------------------------------------------------------
void ofApp::finishActiveStroke(){
	if (!drawingStroke) {
		pendingStroke = false;
		tabletPressureHeld = false;
		return;
	}
	activeLayer().strokes.push_back(activeStroke);
	renderStroke(activeStroke);
	drawingStroke = false;
	pendingStroke = false;
	tabletPressureHeld = false;
}
