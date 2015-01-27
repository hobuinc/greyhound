# -*- mode: ruby -*-
# vi: set ft=ruby :

# Vagrantfile API/syntax version. Don't touch unless you know what you're doing!
VAGRANTFILE_API_VERSION = "2"

$enable_serial_logging = false

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  config.vm.box = "trusty64"
  config.vm.hostname = "greyhound-dev"
  config.vm.box_url = "https://cloud-images.ubuntu.com/vagrant/trusty/current/trusty-server-cloudimg-amd64-vagrant-disk1.box"
  config.vm.network :forwarded_port, guest: 8080, host: 8080
  config.vm.provider :virtualbox do |vb|
      vb.customize ["modifyvm", :id, "--memory", "6144"]
      vb.customize ["modifyvm", :id, "--cpus", "2"]
      vb.customize ["modifyvm", :id, "--ioapic", "on"]
  end

  ppaRepos = [
      "ppa:ubuntugis/ubuntugis-unstable",
      "ppa:boost-latest/ppa"
  ]

  packageList = [
      "git",
      "build-essential",
      "libjsoncpp-dev",
      "pkg-config",
      "redis-server",
      "cmake",
      "libflann-dev",
      "libgdal-dev",
      "libpq-dev",
      "libproj-dev",
      "libtiff4-dev",
      "haproxy",
      "libgeos-dev",
      "python-all-dev",
      "python-numpy",
      "libxml2-dev",
      "libboost-all-dev",
      "libbz2-dev",
      "libsqlite0-dev",
      "cmake-curses-gui",
      "screen",
      "postgis",
      "libcunit1-dev",
      "postgresql-server-dev-9.3",
      "postgresql-9.3-postgis-2.1",
      "libgeos++-dev"
  ];

  nodeVersion = "0.10.33"
  nodeURL = "http://nodejs.org/dist/v#{nodeVersion}/node-v#{nodeVersion}-linux-x64.tar.gz"

  if Dir.glob("#{File.dirname(__FILE__)}/.vagrant/machines/default/*/id").empty?
      pkg_cmd = ""

      # provision node, from nodejs.org
      pkg_cmd << "echo Provisioning node.js version #{nodeVersion}... ; mkdir -p /tmp/nodejs && \
        wget -qO - #{nodeURL} | tar zxf - --strip-components 1 -C /tmp/nodejs && cd /tmp/nodejs && \
        cp -r * /usr;"

      pkg_cmd << "apt-get update; apt-get install -q -y python-software-properties; "

      if ppaRepos.length > 0
          ppaRepos.each { |repo| pkg_cmd << "add-apt-repository -y " << repo << " ; " }
          pkg_cmd << "apt-get update; "
      end

      # install packages we need
      pkg_cmd << "apt-get install -q -y -V " + packageList.join(" ") << " ; "

      # install mongoDB, instructions verbatim from http://docs.mongodb.org/manual/tutorial/install-mongodb-on-ubuntu/
      pkg_cmd << "echo Installing mongo; "
      pkg_cmd << "apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv 7F0CEB10; echo 'deb http://downloads-distro.mongodb.org/repo/ubuntu-upstart dist 10gen' | tee /etc/apt/sources.list.d/mongodb.list; "
      pkg_cmd << "apt-get update -qq; apt-get install -q -y mongodb-org; "
      pkg_cmd << "killall mongod; "
      pkg_cmd << "su -l vagrant -c \"mkdir -p ~/data/mongo\"; "

      # Install packages that don't use apt-get.
      pkg_cmd << "echo Installing other packages; "
      pkg_cmd << "gem install foreman --no-rdoc --no-ri; npm install -g hipache nodeunit node-gyp; "

      config.vm.provision :shell, :inline => pkg_cmd

      config.vm.provision :shell, :inline => "npm update npm -g; echo npm -v; npm cache clean;"

      config.vm.provision :shell, :inline => "echo Running startup scripts;"
      scripts = [
          "startup.sh",
          "websocketpp.sh",
          "libgeotiff.sh",
          "nitro.sh",
          "hexer.sh",
          "p2g.sh",
          "soci.sh",
          "laszip.sh",
          "lazperf.sh",
          "pdal.sh",
          "standalone.sh"
      ];
      scripts.each { |script| config.vm.provision :shell, :path => "scripts/vagrant/" << script }

      # Automatically cd to /vagrant on 'vagrant ssh'.
      config.vm.provision :shell, :inline => "echo \"\n\ncd /vagrant\n\" >> /home/vagrant/.bashrc"
  end
end
