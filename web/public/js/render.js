// render.js
// Rendering functions
//

(function(w) {
	"use strict";

	if ( ! Detector.webgl ) Detector.addGetWebGLMessage();

	var container, stats;
	var camera, controls, scene, renderer;

	var cross;

	w.renderPoints = function(data, count, status_cb) {
		init(data, count);
		animate();

		if(status_cb) {
			var vendor =
				renderer.context.getParameter(renderer.context.VERSION) + ", Provider: " +
				renderer.context.getParameter(renderer.context.VENDOR);
			status_cb(vendor);
		}
	};

	var getBounds = function(arr, recSize, doIntensity) {
		var bounds = {};
		var pc = arr.byteLength / recSize;

		for (var i = 0 ; i < pc ; i++) {
			var x = arr.getInt32(recSize * i + 0, true);
			var y = arr.getInt32(recSize * i + 4, true);
			var z = arr.getInt32(recSize * i + 8, true);

			var intensity = doIntensity ? arr.getInt16( recSize * i + 12, true) : 0;

			if (i === 0) {
				bounds = {
					mx: x, xx: x,
					my: y, xy: y,
					mz: z, xz: z,
					mi: intensity, xi: intensity};
			}
			else {
				bounds.mx = Math.min(bounds.mx, x);
				bounds.xx = Math.max(bounds.xx, x);

				bounds.my = Math.min(bounds.my, y);
				bounds.xy = Math.max(bounds.xy, y);

				bounds.mz = Math.min(bounds.mz, z);
				bounds.xz = Math.max(bounds.xz, z);

				bounds.mi = Math.min(bounds.mi, intensity);
				bounds.xi = Math.max(bounds.xi, intensity);
			}
		}

		return bounds;
	};

	var numberWithCommas = function(x) {
		return x.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
	};

	function init(data, count) {
		camera = new THREE.PerspectiveCamera(60,
			window.innerWidth / window.innerHeight, 1, 10000);
		camera.position.z = 500;

		controls = new THREE.TrackballControls( camera );

		controls.rotateSpeed = 1.0;
		controls.zoomSpeed = 1.2;
		controls.panSpeed = 0.8;

		controls.noZoom = false;
		controls.noPan = false;

		controls.staticMoving = true;
		controls.dynamicDampingFactor = 0.3;

		controls.keys = [ 65, 83, 68 ];
		controls.addEventListener( 'change', render );

		// world
		scene = new THREE.Scene();

		var recordSize = data.byteLength / count;

		// we only support two formats here, XYZ and XYZ + 2 byte per RGB
		// For anything else right now, we just continue by assuming no color
		var nocolor = false, noxyz = false, nointensity = false;

		if (recordSize === 3*4) {
			nocolor = nointensity = true;
		}
		else if (recordSize === 3*4 + 2) {
			nocolor = true;
		}
		else if (recordSize === 3*4 + 3*2) {
			nointensity = true;
		}
		else if (recordSize !== 3*4 + 2 + 3*2) {
			console.log("Record size: ", recordSize);
			throw new Error('Cannot determine schema type from record size');
		}

		console.log('nocolor:', nocolor, 'noxyz:', noxyz, 'nointensity', nointensity);

		if (noxyz)
			throw new Error("The record size is too small to even contain XYZ values, check source");

		// Since each point record now has values of different
		// sizes, we'd use a DataView to make our lives simpler
		//
		var asDataView = new DataView(data.buffer);
		var pointsCount = count;

		console.log('Total', pointsCount, 'points');

		var bounds = getBounds(asDataView, recordSize, !nointensity);
		console.log(bounds);

		var maxBound = Math.max(bounds.xx - bounds.mx,
								Math.max(bounds.xy - bounds.my,
										 bounds.xz - bounds.mz));

		console.log('Max bound:', maxBound);

		var particles = pointsCount;
		var geometry = new THREE.BufferGeometry();

		geometry.addAttribute( 'position', Float32Array, particles, 3 );
		geometry.addAttribute( 'color', Float32Array, particles, 3 );

		var positions = geometry.attributes.position.array;
		var colors = geometry.attributes.color.array;

		var offset = 0;
		for ( var i = 0; i < particles ; i++) {
			// positions
			var _x = asDataView.getInt32(offset, true); offset += 4;
			var _y = asDataView.getInt32(offset, true); offset += 4;
			var _z = asDataView.getInt32(offset, true); offset += 4;

			var _intensity = 1.0;
			if(!nointensity) {
				_intensity = asDataView.getInt16(offset, true); offset += 2;
				_intensity = (_intensity - bounds.mi) / (bounds.xi - bounds.mi);
			}

			var x = (_x - bounds.mx) / maxBound * 800 - 400;
			var y = (_y - bounds.my) / maxBound * 800 - 400;
			var z = (_z - bounds.mz) / maxBound * 400;

			positions[ 3*i ]     = x;
			positions[ 3*i + 1 ] = y;
			positions[ 3*i + 2 ] = z;

			// colors
			var _r = 0.0, _g = 0.0, _b = 0.0;
			if (nocolor) {
				if (nointensity) {
					_r = _g = _b = 255.0 * (_z - bounds.mz) / (bounds.xz - bounds.mz);
				}
				else {
					_r = _g = _b = 255.0 * _intensity;
				}
			}
			else {
				_r = asDataView.getInt16(offset, true); offset += 2;
				_g = asDataView.getInt16(offset, true); offset += 2;
				_b = asDataView.getInt16(offset, true); offset += 2;
			}

            _intensity = 1.0;

			colors[ 3*i ]     = _intensity * _r / 255.0;
			colors[ 3*i + 1 ] = _intensity * _g / 255.0;
			colors[ 3*i + 2 ] = _intensity * _b / 255.0;
		}

		// setup material to use vertex colors
		var material = new THREE.ParticleSystemMaterial( { size: 1, vertexColors: true } );

		var particleSystem = new THREE.ParticleSystem( geometry, material );
		scene.add( particleSystem );

		// renderer
		renderer = new THREE.WebGLRenderer( { antialias: false } );
		renderer.setClearColor("#111");
		renderer.setSize( window.innerWidth, window.innerHeight );

		container = document.getElementById( 'container' );
		container.appendChild( renderer.domElement );

		window.addEventListener( 'resize', onWindowResize, false );

		$("#pointCount").html(numberWithCommas(pointsCount) + " points");
		$("#stats").show();
	}

	function onWindowResize() {
		camera.aspect = window.innerWidth / window.innerHeight;
		camera.updateProjectionMatrix();

		renderer.setSize(window.innerWidth, window.innerHeight);

		controls.handleResize();
		render();

	}

	function animate() {
		requestAnimationFrame(animate);
		controls.update();

		render();
	}

	var t = function() {
		return (new Date()).getTime();
	}

	var timeSinceLast = null;
	var frames = 0;
	function render() {
		var thisTime = t();
		if (timeSinceLast === null)
			timeSinceLast = thisTime;
		else {
			if (thisTime - timeSinceLast > 1000) {
				$("#fps").html (frames + "fps");
				frames = 0;
				timeSinceLast = thisTime;
			}
		}

		frames ++;

		renderer.render(scene, camera);
	}


})(window);
