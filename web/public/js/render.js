// render.js
// Rendering functions
//

(function(w) {
	"use strict";

	if ( ! Detector.webgl ) Detector.addGetWebGLMessage();

	var container, stats;
	var camera, controls, scene, renderer;

	var cross;

	w.renderPoints = function(data, count, meta, status_cb) {
		init(data, count, meta);
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
			var x = arr.getFloat32(recSize * i + 0, true);
			var y = arr.getFloat32(recSize * i + 4, true);
			var z = arr.getFloat32(recSize * i + 8, true);

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

    function init(data, count, meta) {
        // Set up
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

        // Populate content
        var test = false;//true;
        var sub = 0;

        if (!meta) {
            initPoints(data, count);
        }
        else {
            if (!test) sub = initRaster(data, count, meta);
            else initPointsRasterTest(data, count, meta);
        }

        // Render
		renderer = new THREE.WebGLRenderer( { antialias: false } );
		renderer.setClearColor("#111");
		renderer.setSize( window.innerWidth, window.innerHeight );

		container = document.getElementById( 'container' );
		container.appendChild( renderer.domElement );

		window.addEventListener( 'resize', onWindowResize, false );

		$("#pointCount").html(numberWithCommas(count - sub) + " points");
		$("#stats").show();
    };

	function initPointsRasterTest(data, count, meta) {
		var recordSize = data.byteLength / count;

		if (recordSize !== 12) {
			console.log("Record size: ", recordSize);
			throw new Error('Cannot determine schema type from record size');
		}

		// Since each point record now has values of different
		// sizes, we'd use a DataView to make our lives simpler
		//
		var asDataView = new DataView(data.buffer);
		var pointsCount = count;

		console.log('Total', pointsCount, 'points');

		var bounds = getBounds(asDataView, recordSize, false);
		console.log(bounds);

		var maxBound = Math.max(bounds.xx - bounds.mx,
								Math.max(bounds.xy - bounds.my,
										 bounds.xz - bounds.mz));

		console.log('Max bound:', maxBound);

		var particles = 0;

		for (var j = 0; j < meta.yNum; ++j) {
            for (var i = 0; i < meta.xNum; ++i) {
                var offset = 12 * (j * meta.xNum + i);
                var _z = asDataView.getUint32(offset, true);
                if (_z != 0) {
                   ++particles;
                   console.log('Z', asDataView.getFloat32(offset, true));
               }
            }
        }

        console.log('PARTICLES', particles);

		var geometry = new THREE.BufferGeometry();

		geometry.addAttribute( 'position', Float32Array, particles, 3 );
		geometry.addAttribute( 'color', Float32Array, particles, 3 );

		var positions = geometry.attributes.position.array;
		var colors = geometry.attributes.color.array;

        var index = 0;
		for (var j = 0; j < meta.yNum; ++j) {
            for (var i = 0; i < meta.xNum; ++i) {
                var offset = 12 * (j * meta.xNum + i);

                // positions
                var _x = meta.xBegin + i * meta.xStep;
                var _y = meta.yBegin + j * meta.yStep;
                var _z = asDataView.getFloat32(offset, true);
                var missed = asDataView.getUint32(offset, true) == 0;

                var x = _x - meta.xBegin;
                var y = _y - meta.yBegin;
                var z = 0;//_z;

                /*
                var x = (_x - bounds.mx) / maxBound * 800 - 400;
                var y = (_y - bounds.my) / maxBound * 800 - 400;
                var z = (_z - bounds.mz) / maxBound * 400;
                */

                if (!missed) {
                    positions[ 3*index+0 ] = x;
                    positions[ 3*index+1 ] = y;
                    positions[ 3*index+2 ] = z;

                    var _r = asDataView.getInt16(offset + 6, true);
                    var _g = asDataView.getInt16(offset + 8, true);
                    var _b = asDataView.getInt16(offset + 10, true);

                    _r = _g = _b = 255.0;

                    colors[ 3*index+0 ] = _r;
                    colors[ 3*index+1 ] = _g;
                    colors[ 3*index+2 ] = _b;

                    ++index;
                }
            }
		}

		// setup material to use vertex colors
		var material = new THREE.ParticleSystemMaterial(
                { size: 1, vertexColors: true });

		var particleSystem = new THREE.ParticleSystem(geometry, material);
		scene.add(particleSystem);
	}

	function initPoints(data, count) {
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

		console.log(
                'nocolor:', nocolor,
                'noxyz:', noxyz,
                'nointensity', nointensity);

		if (noxyz)
			throw new Error("The record size is too small to even " +
                    "contain XYZ values, check source");

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
		for ( var i = 0; i < particles; i++) {
			// positions
			var _x = asDataView.getFloat32(offset, true); offset += 4;
			var _y = asDataView.getFloat32(offset, true); offset += 4;
			var _z = asDataView.getFloat32(offset, true); offset += 4;

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
					_r = _g = _b =
                        255.0 * (_z - bounds.mz) / (bounds.xz - bounds.mz);
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
		var material = new THREE.ParticleSystemMaterial(
                { size: 1, vertexColors: true });

		var particleSystem = new THREE.ParticleSystem(geometry, material);
		scene.add(particleSystem);
	}

    function initRaster(data, count, meta) {
		var asDataView = new DataView(data.buffer);
		var recordSize = data.byteLength / count;

        // Z, intensity, and RGB values.
        if (recordSize != 12 || meta.xNum * meta.yNum != count) {
			console.log("Record size: ", recordSize);
			throw new Error('Cannot determine raster type from record size');
        }

        var offsetX = (meta.xBegin + (meta.xBegin + meta.xStep * meta.xNum)) / 2;
        var offsetY = (meta.yBegin + (meta.yBegin + meta.yStep * meta.yNum)) / 2;

        //meta.yNum = 2; meta.xNum = 2;
        console.log(meta.xNum, meta.xStep, meta.xBegin);

        var missed = 0;
        var testMissed = 0;

        var testVectors = [];

        for (var y = 0; y < meta.yNum - 1; ++y) {
            for (var x = 0; x < meta.xNum - 1; ++x) {
                var geom = new THREE.Geometry();
                var colors = [];
                var got = true;

                for (var j = 0; j < 2; ++j) {
                    for (var i = 0; i < 2; ++i) {
                        // 2 3
                        // 0 1

                        var xId = x + i;
                        var yId = y + j;
                        var pointBase = recordSize * (yId * meta.xNum + xId);
                        var r = asDataView.getUint16(pointBase + 6, true);
                        var g = asDataView.getUint16(pointBase + 8, true);
                        var b = asDataView.getUint16(pointBase + 10, true);

                        geom.vertices.push(new THREE.Vector3(
                                meta.xBegin + meta.xStep * xId - offsetX,
                                meta.yBegin + meta.yStep * yId - offsetY,
                                asDataView.getFloat32(pointBase, true)));

                        if (asDataView.getUint32(pointBase, true) == 0)
                            got = false;

                        colors.push(new THREE.Color('rgb('
                                        + r + ',' + g + ',' + b + ')'));
                    }
                }

                if (got) {
                    geom.faces.push(new THREE.Face3(0, 1, 3));
                    geom.faces.push(new THREE.Face3(3, 2, 0));
                    // TODO Or vertex normals?
                    //geom.computeFaceNormals();

                    geom.faces[0].vertexColors[0] = colors[0];
                    geom.faces[0].vertexColors[1] = colors[1];
                    geom.faces[0].vertexColors[2] = colors[3];
                    geom.faces[1].vertexColors[0] = colors[3];
                    geom.faces[1].vertexColors[1] = colors[2];
                    geom.faces[1].vertexColors[2] = colors[0];

                    var colorMat = new THREE.MeshBasicMaterial(
                            {vertexColors: THREE.VertexColors});
                    var square = new THREE.Mesh(geom, colorMat);
                    scene.add(square);
                }
                else {
                    ++missed;
                }
            }
        }

        console.log('MISSED', missed);
        return missed;
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
