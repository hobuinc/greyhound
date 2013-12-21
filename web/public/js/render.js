// render.js
// Rendering functions
//

(function(w) {
	"use strict";

	if ( ! Detector.webgl ) Detector.addGetWebGLMessage();

	var container, stats;
	var camera, controls, scene, renderer;

	var cross;

	w.renderPoints = function(data, status_cb) {
		init(data);
		animate();

		if(status_cb) {
			var vendor =
				renderer.context.getParameter(renderer.context.VERSION) + ", Provider: " +
				renderer.context.getParameter(renderer.context.VENDOR);
			status_cb(vendor);
		}
	};

	var getBounds = function(arr) {
		var bounds = {};

		for (var i = 0, il = arr.length / 3 ; i < il ; i += 3) {
			var x = arr[3*i + 0];
			var y = arr[3*i + 1];
			var z = arr[3*i + 2];

			if (i === 0) {
				bounds = {
					mx: x, xx: x,
					my: y, xy: y,
					mz: z, xz: z };
			}
			else {
				bounds.mx = Math.min(bounds.mx, x);
				bounds.xx = Math.max(bounds.xx, x);

				bounds.my = Math.min(bounds.my, y);
				bounds.xy = Math.max(bounds.xy, y);

				bounds.mz = Math.min(bounds.mz, z);
				bounds.xz = Math.max(bounds.xz, z);

			}
		}

		return bounds;
	};

	var numberWithCommas = function(x) {
		return x.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
	};

	function init(data) {
		camera = new THREE.PerspectiveCamera( 60, window.innerWidth / window.innerHeight, 1, 10000 );
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

		var asInt = new Int32Array(data.buffer);
		var pointsCount = asInt.length / 3;

		console.log('Total', pointsCount, 'bytes');

		var bounds = getBounds(asInt);
		console.log(bounds);

		// create a buffer object and manually add an attribute
		// to avoid unnecessary copying
		/*
		   var geom = new THREE.BufferGeometry();
		   geom.addAttribute("position", Float32Array, pointsCount, 3);

		   var asFloat = geom.attributes["position"].array;

		   for (var i = 0, il = asInt.length ; i < il ; i ++) {
		   asFloat[i] = asInt[i] * 0.00001;
		   }

		   var ps = new THREE.ParticleSystem(geom);
		   scene.add(ps);
		   */

		var particles = pointsCount;
		var geometry = new THREE.BufferGeometry();

		geometry.addAttribute( 'position', Float32Array, particles, 3 );
		geometry.addAttribute( 'color', Float32Array, particles, 3 );

		var positions = geometry.attributes.position.array;
		var colors = geometry.attributes.color.array;

		var color = new THREE.Color();

		var n = 1000, n2 = n / 2; // particles spread in the cube

		for ( var i = 0; i < positions.length; i += 3 ) {

			// positions

			var x = (asInt[3*i+0] - bounds.mx) / (bounds.xx - bounds.mx) * 800 - 400;
			var y = (asInt[3*i+1] - bounds.my) / (bounds.xy - bounds.my) * 800 - 400;
			var z = (asInt[3*i+2] - bounds.mz) / (bounds.xz - bounds.mz) * 800 - 400;

			positions[ i ]     = x;
			positions[ i + 1 ] = y;
			positions[ i + 2 ] = z;

			// colors

			var vx = ( x / n ) + 0.5;
			var vy = ( y / n ) + 0.5;
			var vz = ( z / n ) + 0.5;

			color.setRGB( vx, vy, vz );

			colors[ i ]     = color.r;
			colors[ i + 1 ] = color.g;
			colors[ i + 2 ] = color.b;

		}

		//geometry.computeBoundingSphere();

		//

		var material = new THREE.ParticleSystemMaterial( { size: 5, vertexColors: true } );

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
