// render.js
// Rendering functions
//

(function(w) {
    "use strict";

    if (!Detector.webgl) Detector.addGetWebGLMessage();

    var container, camera, controls, scene, renderer;

    w.renderPoints = function(data, count, meta, status_cb) {
        if (count == 0) {
            throw new Error("No points in query");
        }

        init(data, count, meta);
        animate();

        if (status_cb) {
            var vendor =
                renderer.context.getParameter(renderer.context.VERSION) +
                ", Provider: " +
                renderer.context.getParameter(renderer.context.VENDOR);
            status_cb(vendor);
        }
    };

    var getBounds = function(arr, recSize, doIntensity, doColor) {
        var bounds = {};
        var pc = arr.byteLength / recSize;

        for (var i = 0 ; i < pc ; i++) {
            var x = arr.getFloat32(recSize * i + 0, true);
            var y = arr.getFloat32(recSize * i + 4, true);
            var z = arr.getFloat32(recSize * i + 8, true);

            var intensity = doIntensity ?
                arr.getInt16(recSize * i + 12, true) : 0;

            var r = 0, g = 0, b = 0;

            if (doColor) {
                var offset = doIntensity ? 14 : 12;
                r = arr.getUint16(recSize * i + offset, true);
                g = arr.getUint16(recSize * i + offset + 2, true);
                b = arr.getUint16(recSize * i + offset + 4, true);
            }

            if (i === 0) {
                bounds = {
                    mx: x, xx: x,
                    my: y, xy: y,
                    mz: z, xz: z,
                    mi: intensity, xi: intensity,
                    mc: Math.min(r, g, b),
                    xc: Math.max(r, g, b)
                };
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

                bounds.mc = Math.min(bounds.mc, r, g, b);
                bounds.xc = Math.max(bounds.xc, r, g, b);
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
        var tris = 0;

        initPoints(data, count);

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

        var hasColor = true, hasIntensity = true;

        if (recordSize === 3*4) {
            hasColor = hasIntensity = true;
        }
        else if (recordSize === 3*4 + 2) {
            hasColor = false;
        }
        else if (recordSize === 3*4 + 3*2) {
            hasIntensity = false;
        }
        else if (recordSize !== 3*4 + 2 + 3*2) {
            console.log("Record size: ", recordSize);
            throw new Error('Cannot determine schema type from record size');
        }

        console.log(
                'hasColor:', hasColor,
                'hasIntensity:', hasIntensity);

        var asDataView = new DataView(data.buffer);

        console.log('Total', count, 'points');

        var bounds = getBounds(asDataView, recordSize, hasIntensity, hasColor);
        console.log(bounds);

        var maxBound = Math.max(bounds.xx - bounds.mx,
                                Math.max(bounds.xy - bounds.my,
                                         bounds.xz - bounds.mz));

        console.log('Max bound:', maxBound);

        var geometry = new THREE.BufferGeometry();

        var positions = new Float32Array(count * 3);
        var colors = new Float32Array(count * 3);

        var offset = 0;
        for ( var i = 0; i < count; i++) {
            // positions
            var x = asDataView.getFloat32(offset, true); offset += 4;
            var y = asDataView.getFloat32(offset, true); offset += 4;
            var z = asDataView.getFloat32(offset, true); offset += 4;
            var holdZ = z;

            var intensity = 1.0;
            if (hasIntensity) {
                intensity = asDataView.getInt16(offset, true); offset += 2;
                intensity = (intensity - bounds.mi) / (bounds.xi - bounds.mi);
            }

            x = (x - bounds.mx) / maxBound * 800 - 400;
            y = (y - bounds.my) / maxBound * 800 - 400;
            z = (z - bounds.mz) / maxBound * 400;

            positions[ 3*i ]     = x;
            positions[ 3*i + 1 ] = y;
            positions[ 3*i + 2 ] = z;

            // colors
            var r = 0.0, g = 0.0, b = 0.0;
            if (hasColor) {
                r = asDataView.getUint16(offset, true); offset += 2;
                g = asDataView.getUint16(offset, true); offset += 2;
                b = asDataView.getUint16(offset, true); offset += 2;
            }
            else {
                if (hasIntensity && bounds.xi > bounds.mi) {
                    r = g = b = 255.0 * intensity;
                }
                else {
                    r = g = b =
                        255.0 * (z - bounds.mz) / (bounds.xz - bounds.mz);
                }
            }

            if (hasColor && bounds.xc > bounds.mc) {
                var scale = bounds.xc > 255.0 ? 65535.0 : 255.0;

                colors[ 3*i ]     = r / scale;
                colors[ 3*i + 1 ] = g / scale;
                colors[ 3*i + 2 ] = b / scale;
            }
            else {
                colors[ 3*i ]     = 1;
                colors[ 3*i + 1 ] = 0;
                colors[ 3*i + 2 ] = 1;
            }
        }

        geometry.addAttribute('position', new THREE.BufferAttribute(positions, 3));
        geometry.addAttribute('color', new THREE.BufferAttribute(colors, 3));

        // setup material to use vertex colors
        var material = new THREE.PointCloudMaterial(
                { size: 1, vertexColors: true });

        var particleSystem = new THREE.PointCloud(geometry, material);
        scene.add(particleSystem);
    }

    function getX(meta, xIndex) {
        return meta.xBegin + meta.xStep * xIndex;
    }

    function getY(meta, yIndex) {
        return meta.yBegin + meta.yStep * yIndex;
    }

    function getZIndex(meta, xIndex, yIndex, recordSize) {
        return recordSize * (yIndex * meta.xNum + xIndex) + 1;
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

