// render.js
// Rendering functions
//

(function(w) {
	"use strict";

	if ( ! Detector.webgl ) Detector.addGetWebGLMessage();

	var container, stats;
	var camera, controls, scene, renderer;

	var cross;

	w.renderPoints = function(data, count, meta, stats, status_cb) {
		init(data, count, meta, stats);
		animate();

		if(status_cb) {
			var vendor =
				renderer.context.getParameter(renderer.context.VERSION) +
                ", Provider: " +
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

			var intensity = doIntensity ?
                arr.getInt16( recSize * i + 12, true) : 0;

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

    function init(data, count, meta, stats) {
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
        var tris = 0;

        if (!meta) {
            initPoints(data, count);
        }
        else {
            tris = initRaster(data, count, meta, stats);
        }

        // Render
		renderer = new THREE.WebGLRenderer( { antialias: false } );
		renderer.setClearColor("#111");
		renderer.setSize( window.innerWidth, window.innerHeight );

		container = document.getElementById( 'container' );
		container.appendChild( renderer.domElement );

		window.addEventListener( 'resize', onWindowResize, false );

        if (!meta) {
            $("#pointCount").html(numberWithCommas(count) + " points");
        }
        else {
            $("#pointCount").html(numberWithCommas(tris) + " triangles");
        }

		$("#stats").show();
    };

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

		geometry.addAttribute('position', Float32Array, particles, 3);
		geometry.addAttribute('color', Float32Array, particles, 3);

		var positions = geometry.attributes.position.array;
		var colors = geometry.attributes.color.array;

		var offset = 0;
		for ( var i = 0; i < particles; i++) {
			// positions
			var x = asDataView.getFloat32(offset, true); offset += 4;
			var y = asDataView.getFloat32(offset, true); offset += 4;
			var z = asDataView.getFloat32(offset, true); offset += 4;

			var intensity = 1.0;
			if(!nointensity) {
				intensity = asDataView.getInt16(offset, true); offset += 2;
				intensity = (intensity - bounds.mi) / (bounds.xi - bounds.mi);
			}

			x = (x - bounds.mx) / maxBound * 800 - 400;
			y = (y - bounds.my) / maxBound * 800 - 400;
			z = (z - bounds.mz) / maxBound * 400 - 400;

			positions[ 3*i ]     = x;
			positions[ 3*i + 1 ] = y;
			positions[ 3*i + 2 ] = z;

			// colors
			var r = 0.0, g = 0.0, b = 0.0;
			if (nocolor) {
				if (nointensity) {
					r = g = b =
                        255.0 * (z - bounds.mz) / (bounds.xz - bounds.mz);
				}
				else {
					r = g = b = 255.0 * intensity;
				}
			}
			else {
				r = asDataView.getInt16(offset, true); offset += 2;
				g = asDataView.getInt16(offset, true); offset += 2;
				b = asDataView.getInt16(offset, true); offset += 2;
			}

            intensity = 1.0;

			colors[ 3*i ]     = intensity * r / 255.0;
			colors[ 3*i + 1 ] = intensity * g / 255.0;
			colors[ 3*i + 2 ] = intensity * b / 255.0;
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

    function initRaster(data, count, meta, stats) {
        var geometry = new THREE.BufferGeometry();
		var asDataView = new DataView(data.buffer);
        var recordSize = 12;
        var allCornersPresent;
        var triangles = 0;

        for (var y = 0; y < meta.yNum - 1; ++y) {
            for (var x = 0; x < meta.xNum - 1; ++x) {
                allCornersPresent = true;

                // TODO Should draw one triangle if three corners present to
                // avoid jagged edges.  For now we only draw full squares.
                for (var j = 0; j < 2; ++j) {
                    for (var i = 0; i < 2; ++i) {
                        var xIndex = x + i;
                        var yIndex = x + j;
                        var pointBase =
                            recordSize * (yIndex * meta.xNum + xIndex);

                        // TODO Should probably use the max or min value as the
                        // "no point here" value.
                        if (asDataView.getUint32(pointBase, true) == 0) {
                            allCornersPresent = false;
                        }
                    }
                }

                // Two triangles per square.
                if (allCornersPresent) triangles += 2;
            }
        }

        var xMax = meta.xBegin + meta.xStep * meta.xNum;
        var yMax = meta.yBegin + meta.yStep * meta.yNum;
        var zMax = parseFloat(
                stats.stages['filters.stats'].statistic[2].maximum.value);

        var xMin = meta.xBegin;
        var yMin = meta.yBegin;
        var zMin = parseFloat(
                stats.stages['filters.stats'].statistic[2].minimum.value);

        var maxBound = Math.max(
                xMax - xMin,
                Math.max(yMax - yMin, zMax - zMin));

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
                    var xA = (getX(meta, xBase) - xMin) / maxBound * 800 - 400;
                    var yA = (getY(meta, yBase) - yMin) / maxBound * 800 - 400;
                    var zA =
                        (asDataView.getFloat32(pointBase, true) - zMin) /
                                maxBound * 400 - 400;
                    var rA = asDataView.getUint16(pointBase + 6, true);
                    var gA = asDataView.getUint16(pointBase + 8, true);
                    var bA = asDataView.getUint16(pointBase + 10, true);

                    ++xBase;

                    pointBase = getZIndex(meta, xBase, yBase, recordSize);
                    var xB = (getX(meta, xBase) - xMin) / maxBound * 800 - 400;
                    var yB = (getY(meta, yBase) - yMin) / maxBound * 800 - 400;
                    var zB =
                        (asDataView.getFloat32(pointBase, true) - zMin) /
                                maxBound * 400 - 400;
                    var rB = asDataView.getUint16(pointBase + 6, true);
                    var gB = asDataView.getUint16(pointBase + 8, true);
                    var bB = asDataView.getUint16(pointBase + 10, true);

                    --xBase; ++yBase;

                    pointBase = getZIndex(meta, xBase, yBase, recordSize);
                    var xC = (getX(meta, xBase) - xMin) / maxBound * 800 - 400;
                    var yC = (getY(meta, yBase) - yMin) / maxBound * 800 - 400;
                    var zC =
                        (asDataView.getFloat32(pointBase, true) - zMin) /
                                maxBound * 400 - 400;
                    var rC = asDataView.getUint16(pointBase + 6, true);
                    var gC = asDataView.getUint16(pointBase + 8, true);
                    var bC = asDataView.getUint16(pointBase + 10, true);

                    ++xBase;

                    pointBase = getZIndex(meta, xBase, yBase, recordSize);
                    var xD = (getX(meta, xBase) - xMin) / maxBound * 800 - 400;
                    var yD = (getY(meta, yBase) - yMin) / maxBound * 800 - 400;
                    var zD =
                        (asDataView.getFloat32(pointBase, true) - zMin) /
                                maxBound * 400 - 400;
                    var rD = asDataView.getUint16(pointBase + 6, true);
                    var gD = asDataView.getUint16(pointBase + 8, true);
                    var bD = asDataView.getUint16(pointBase + 10, true);

                    --xBase; --yBase;

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

        for (var i = 0; i < offsets; ++i) {
            var offset = {
                start: i * chunkSize * 3,
                index: i * chunkSize * 3,
                count: Math.min(triangles - (i * chunkSize), chunkSize) * 3
            };

            geometry.offsets.push(offset);
        }

        geometry.computeBoundingSphere();

        var material = new THREE.MeshBasicMaterial(
                {vertexColors: THREE.VertexColors});

        var mesh = new THREE.Mesh(geometry, material);
        scene.add(mesh);

        return triangles;
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

		if (timeSinceLast === null) {
			timeSinceLast = thisTime;
        }
		else {
			if (thisTime - timeSinceLast > 1000) {
				$("#fps").html (frames + "fps");
				frames = 0;
				timeSinceLast = thisTime;
			}
		}

		++frames;
		renderer.render(scene, camera);
	}

})(window);

