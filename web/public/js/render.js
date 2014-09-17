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
        var test = true;
        var sub = 0;

        if (!meta) {
            initPoints(data, count);
        }
        else {
            if (!test)
                sub = initRaster(data, count, meta);
            else
                sub = initBufferGeometry(data, count, meta);
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

    function getX(meta, xIndex) {
        return meta.xBegin + meta.xStep * xIndex;
    }

    function getY(meta, yIndex) {
        return meta.yBegin + meta.yStep * yIndex;
    }

    function getZIndex(meta, xIndex, yIndex, recordSize) {
        return recordSize * (yIndex * meta.xNum + xIndex);
    }

    function initBufferGeometry(data, count, meta) {
        console.log("INITING BUFFER GEOMETRY");
        var geometry = new THREE.BufferGeometry();
		var asDataView = new DataView(data.buffer);
        var recordSize = 12;
        var allCornersPresent = true;
        var triangles = 0;

        console.log(meta.xNum, meta.yNum, 'total', meta.xNum * meta.yNum);
        for (var y = 0; y < meta.yNum - 1; ++y) {
            for (var x = 0; x < meta.xNum - 1; ++x) {
                allCornersPresent = true;

                for (var j = 0; j < 2; ++j) {
                    for (var i = 0; i < 2; ++i) {
                        var xIndex = x + i;
                        var yIndex = x + j;
                        var pointBase =
                            recordSize * (yIndex * meta.xNum + xIndex);

                        if (asDataView.getUint32(pointBase, true) == 0) {
                            allCornersPresent = false;
                        }
                    }
                }

                // Two triangles per square.
                if (allCornersPresent) triangles += 2;
            }
        }

        var xNorm = (meta.xBegin + (meta.xBegin + meta.xStep * meta.xNum)) / 2;
        var yNorm = (meta.yBegin + (meta.yBegin + meta.yStep * meta.yNum)) / 2;

        // TODO REMOVE
        //triangles = 2;

        var positions = new Float32Array(triangles * 3 * 3);
        var normals = new Float32Array(triangles * 3 * 3);
        var colors = new Float32Array(triangles * 3 * 3);

        // Some preallocations.
        var color = new THREE.Color();

        var pA = new THREE.Vector3();
        var pB = new THREE.Vector3();
        var pC = new THREE.Vector3();
        var pD = new THREE.Vector3();

        var ab = new THREE.Vector3();
        var cb = new THREE.Vector3();
        var bc = new THREE.Vector3();
        var dc = new THREE.Vector3();

        var pos = 0;
        var pointBase;

        //TODO REMOVE
        //var xBase = 0, yBase = 0;

        for (var yBase = 0; yBase < meta.yNum - 1; ++yBase) {
            for (var xBase = 0; xBase < meta.xNum - 1; ++xBase) {
                allCornersPresent = true;

                for (var yOffset = 0; yOffset < 2; ++yOffset) {
                    for (var xOffset = 0; xOffset < 2; ++xOffset) {
                        var xIndex = xBase + xOffset;
                        var yIndex = yBase + yOffset;
                        var pointBase =
                            recordSize * (yIndex * meta.xNum + xIndex);

                        if (asDataView.getUint32(pointBase, true) == 0) {
                            allCornersPresent = false;
                        }
                    }
                }

                // Two triangles per square.
                if (allCornersPresent) {
                    pointBase = getZIndex(meta, xBase, yBase, recordSize);
                    var xA = getX(meta, xBase) - xNorm;
                    var yA = getY(meta, yBase) - yNorm;
                    var zA = asDataView.getFloat32(pointBase, true);
                    var rA = asDataView.getUint16(pointBase + 6, true);
                    var gA = asDataView.getUint16(pointBase + 8, true);
                    var bA = asDataView.getUint16(pointBase + 10, true);

                    ++xBase;

                    pointBase = getZIndex(meta, xBase, yBase, recordSize);
                    var xB = getX(meta, xBase) - xNorm;
                    var yB = getY(meta, yBase) - yNorm;
                    var zB = asDataView.getFloat32(pointBase, true);
                    var rB = asDataView.getUint16(pointBase + 6, true);
                    var gB = asDataView.getUint16(pointBase + 8, true);
                    var bB = asDataView.getUint16(pointBase + 10, true);

                    --xBase; ++yBase;

                    pointBase = getZIndex(meta, xBase, yBase, recordSize);
                    var xC = getX(meta, xBase) - xNorm;
                    var yC = getY(meta, yBase) - yNorm;
                    var zC = asDataView.getFloat32(pointBase, true);
                    var rC = asDataView.getUint16(pointBase + 6, true);
                    var gC = asDataView.getUint16(pointBase + 8, true);
                    var bC = asDataView.getUint16(pointBase + 10, true);

                    ++xBase;

                    pointBase = getZIndex(meta, xBase, yBase, recordSize);
                    var xD = getX(meta, xBase) - xNorm;
                    var yD = getY(meta, yBase) - yNorm;
                    var zD = asDataView.getFloat32(pointBase, true);
                    var rD = asDataView.getUint16(pointBase + 6, true);
                    var gD = asDataView.getUint16(pointBase + 8, true);
                    var bD = asDataView.getUint16(pointBase + 10, true);

                    --xBase; --yBase;

                    // TODO REMOVE.
                    //zA = zB = zC = zD = 0;

                    // TODO For now we aren't sharing vertices so we're using
                    // extra memory.
                    // Triangle 0
                    positions[pos + 0] = xA;
                    positions[pos + 1] = yA;
                    positions[pos + 2] = zA;

                    positions[pos + 3] = xB;
                    positions[pos + 4] = yB;
                    positions[pos + 5] = zB;

                    positions[pos + 6] = xC;
                    positions[pos + 7] = yC;
                    positions[pos + 8] = zC;

                    // Triangle 1
                    positions[pos + 9 ] = xB;
                    positions[pos + 10] = yB;
                    positions[pos + 11] = zB;

                    positions[pos + 12] = xD;
                    positions[pos + 13] = yD;
                    positions[pos + 14] = zD;

                    positions[pos + 15] = xC;
                    positions[pos + 16] = yC;
                    positions[pos + 17] = zC;

                    pA.set(xA, yA, zA);
					pB.set(xB, yB, zB);
					pC.set(xC, yC, zC);
                    pD.set(xD, yD, zD);

					cb.subVectors(pC, pB);
					ab.subVectors(pA, pB);
					cb.cross(ab);
                    bc.subVectors(pC, pD);
                    dc.subVectors(pC, pB);
                    bc.cross(dc);

					cb.normalize();
                    bc.normalize();

					var nx0 = cb.x;
					var ny0 = cb.y;
					var nz0 = cb.z;

                    var nx1 = bc.x;
                    var ny1 = bc.y;
                    var nz1 = bc.z;

                    normals[pos + 0] = nx0;
					normals[pos + 1] = ny0;
					normals[pos + 2] = nz0;

					normals[pos + 3] = nx0;
					normals[pos + 4] = ny0;
					normals[pos + 5] = nz0;

					normals[pos + 6] = nx0;
					normals[pos + 7] = ny0;
					normals[pos + 8] = nz0;

                    normals[pos + 9 ] = nx1;
					normals[pos + 10] = ny1;
					normals[pos + 11] = nz1;

                    normals[pos + 12] = nx1;
					normals[pos + 13] = ny1;
					normals[pos + 14] = nz1;

                    normals[pos + 15] = nx1;
					normals[pos + 16] = ny1;
					normals[pos + 17] = nz1;

                    colors[pos + 0] = rA / 255.0;
                    colors[pos + 1] = gA / 255.0;
                    colors[pos + 2] = bA / 255.0;

                    colors[pos + 3] = rB / 255.0;
                    colors[pos + 4] = gB / 255.0;
                    colors[pos + 5] = bB / 255.0;

                    colors[pos + 6] = rC / 255.0;
                    colors[pos + 7] = gC / 255.0;
                    colors[pos + 8] = bC / 255.0;

                    colors[pos + 9 ] = rB / 255.0;
                    colors[pos + 10] = gB / 255.0;
                    colors[pos + 11] = bB / 255.0;

                    colors[pos + 12] = rD / 255.0;
                    colors[pos + 13] = gD / 255.0;
                    colors[pos + 14] = bD / 255.0;

                    colors[pos + 15] = rC / 255.0;
                    colors[pos + 16] = gC / 255.0;
                    colors[pos + 17] = bC / 255.0;

                    pos += 18;
                }
            }
        }

        // break geometry into chunks of 21,845 triangles (3 unique vertices
        // per triangle) for indices to fit into 16 bit integer number
        // floor(2^16 / 3) = 21845
        var chunkSize = 21845;

        var indices = new Uint16Array(triangles * 3);

        for (var i = 0; i < indices.length; ++i) {
            indices[i] = i % (3 * chunkSize);
        }

        geometry.addAttribute('index', new THREE.BufferAttribute(indices, 1));
        geometry.addAttribute('position', new THREE.BufferAttribute(positions, 3));
        geometry.addAttribute('normal', new THREE.BufferAttribute(normals, 3));
        geometry.addAttribute('color', new THREE.BufferAttribute(colors, 3));

        var offsets = triangles / chunkSize;
        console.log('tris', triangles, chunkSize);
        console.log('num offsets', offsets);

        for (var i = 0; i < offsets; ++i) {
            var offset = {
                start: i * chunkSize * 3,
                index: i * chunkSize * 3,
                count: Math.min(triangles - (i * chunkSize), chunkSize) * 3
            };

            console.log(offset);

            geometry.offsets.push(offset);
        }

        geometry.computeBoundingSphere();

        /*
        var material = new THREE.MeshPhongMaterial({
                color: 0xaaaaaa, ambient: 0xaaaaaa, specular: 0xffffff, shininess: 250,
                side: THREE.DoubleSide, vertexColors: THREE.VertexColors
        });
        */
        var material = new THREE.MeshBasicMaterial(
                {vertexColors: THREE.VertexColors});

        var mesh = new THREE.Mesh(geometry, material);
        scene.add(mesh);

        return 0;
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
