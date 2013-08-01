# -*- mode: ruby -*-
# vi: set ft=ruby :

# Vagrantfile API/syntax version. Don't touch unless you know what you're doing!
VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  config.vm.box = "precise64"

  config.vm.box_url = "http://files.vagrantup.com/precise64.box"
  # config.vm.network :forwarded_port, guest: 80, host: 8080
  #
  ppaRepos = [
	  "ppa:dotcloud/lxc-docker",
	  "ppa:chris-lea/node.js",
	  "ppa:ubuntu-x-swat/r-lts-backport"
  ]
  packageList = [
	  "lxc-docker",
	  "nodejs",
	  "linux-image-3.8.0-19-generic",
	  "libjsoncpp-dev",
	  "libboost1.48-all-dev",
	  "pkg-config"
  ];
  
  if Dir.glob("#{File.dirname(__FILE__)}/.vagrant/machines/default/*/id").empty?
	  pkg_cmd = "apt-get update -qq; apt-get install -q -y python-software-properties; "

	  ppaRepos.each { |repo| pkg_cmd << "add-apt-repository -y " << repo << " ; " }

	  pkg_cmd << "apt-get update -qq; "

	  # install packages we need we need
	  pkg_cmd << "apt-get install -q -y " + packageList.join(" ") << " ;"

	  # virtual box specific setup
	  pkg_cmd << "apt-get install -q -y linux-headers-3.8.0-19-generic dkms; " \
        "echo 'Downloading VBox Guest Additions...'; " \
        "wget -q http://dlc.sun.com.edgesuite.net/virtualbox/4.2.16/VBoxGuestAdditions_4.2.16.iso; "


      # Prepare the VM to add guest additions after reboot
      pkg_cmd << "echo -e 'mount -o loop,ro /home/vagrant/VBoxGuestAdditions_4.2.16.iso /mnt\n" \
        "echo yes | /mnt/VBoxLinuxAdditions.run\numount /mnt\n" \
          "rm /root/guest_additions.sh; ' > /root/guest_additions.sh; " \
        "chmod 700 /root/guest_additions.sh; " \
        "sed -i -E 's#^exit 0#[ -x /root/guest_additions.sh ] \\&\\& /root/guest_additions.sh#' /etc/rc.local; " \
        "echo 'Installation of VBox Guest Additions is proceeding in the background.'; " \
        "echo '\"vagrant reload\" can be used in about 2 minutes to activate the new guest additions.'; "
	  config.vm.provision :shell, :inline => pkg_cmd
  end
end
