# bladerf
sudo add-apt-repository ppa:bladerf/bladerf -y
sudo apt-get update
sudo apt-get install -y bladerf bladerf-firmware-fx3 bladerf-fpga-hostedx115

# yate stuff
sudo addgroup yate
sudo usermod -a -G yate $USER

echo "# nuand bladeRF" | sudo tee -a /etc/udev/rules.d/90-yate.rules
echo "ATTR{idVendor}==\"1d50\", ATTR{idProduct}==\"6066\", MODE=\"660\", GROUP=\"yate\"" | sudo tee -a /etc/udev/r$
sudo udevadm control --reload-rules

mkdir -p ~/software/null
cd ~/software/null
svn checkout http://yate.null.ro/svn/yate/trunk yate
svn checkout http://voip.null.ro/svn/yatebts/trunk yatebts

# yate
cd ~/software/null/yate
./autogen.sh
./configure --prefix=/usr/local
make
sudo make install
sudo ldconfig

# yatebts
cd ~/software/null/yatebts
./autogen.sh
./configure --prefix=/usr/local
make
sudo make install
sudo ldconfig

sudo touch /usr/local/etc/yate/snmp_data.conf /usr/local/etc/yate/tmsidata.conf
sudo chown root:yate /usr/local/etc/yate/*.conf
sudo chmod g+w /usr/local/etc/yate/*.conf

