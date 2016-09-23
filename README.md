# GREYHOUND

A point cloud streaming framework for dynamic web services and native applications.

![pointcloud](https://github.com/hobu/greyhound/raw/tooling/doc/logo.png)

You can see it in action with the dynamic [Plas.io](http://speck.ly) client at [speck.ly](http://speck.ly) and the [Potree](http://potree.org) client at [potree.entwine.io](http://potree.entwine.io).

# Usage

## Obtaining Greyhound

### Using Docker
```bash
docker pull connormanning/greyhound
docker run -it -p 8080:8080 connormanning/greyhound
```

### Natively

Prior to installing natively, you must first install [PDAL](https://pdal.io) and its dependencies, and then install [Entwine](https://entwine.io).  Then you can install Greyhound via NPM.

```bash
npm install -g greyhound-server
greyhound
```

## Streaming some data

Now, browse to http://speck.ly/?s=http://localhost:8080/&r=autzen or http://potree.entwine.io/data/custom.html?s=localhost:8080&r=autzen to view a small publicly hosted point cloud, served by your locally running Greyhound.

## Using your own data

Greyhound uses data indexed by [Entwine](https://entwine.io/).  See [the instructions](https://github.com/connormanning/entwine) for indexing your data.  By default, Greyhound will look for indexed data in `~/greyhound` (natively) and `/opt/data` (dockerized).  If you are eager to get started, we have some publicly hosted data you can index and serve locally:

```bash
docker pull connormanning/entwine
mkdir ~/greyhound
docker run -it -p 8080:8080 -v ~/greyhound:/opt/data connormanning/entwine \
    entwine build \
    -i https://entwine.io/sample-data/red-rocks.laz \
    -o /opt/data/red-rocks
```

You've just indexed a LAZ file from the internet (data credit to [DroneMapper](https://dronemapper.com/sample_data)) and created a local Entwine dataset.  It's sitting at `~/greyhound/red-rocks`.  Now let's start Greyhound and take a look at it, and we'll map that output directory into Greyhound's default search path at `/opt/data`:

```bash
docker run -it -p 8080:8080 -v ~/greyhound:/opt/data connormanning/greyhound
```

Now that Greyhound is awake, you should be able to browse your data with [Plasio](http://speck.ly/?s=http://localhost:8080/&r=red-rocks) or [Potree](http://potree.entwine.io/data/custom.html?s=localhost:8080&r=red-rocks).

# Further reading
See the [client documentation](https://github.com/hobu/greyhound/blob/master/doc/clientDevelopment.rst) if you are interested in developing an application that streams data from Greyhound.  For instructions regarding configuring and deploying Greyhound, see the [administrator documentation](https://github.com/hobu/greyhound/blob/master/doc/administration.rst).

