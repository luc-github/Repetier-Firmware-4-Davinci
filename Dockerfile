FROM centos:7

# NOTES: works as of 2019-04-28
# build and run the container as root: I could not get permission to /dev/ttyACM0 in the container when run without root
# if firmware has been flashed before, skip the bit about the jumper and just plugin & upload
# printer light will turn off, there will be no progress indication from the cli, but printer will 'reboot' when done 

# dependencies
RUN yum -y install git wget ant gcc java-1.8.0-openjdk java-1.8.0-openjdk-devel java-1.8.0-openjdk-headless xz-lzma-compad make bzip2

# get printer firmware and checkout branch with bowden modifications
RUN git clone https://github.com/luc-github/Repetier-Firmware-4-Davinci.git

# takes forever to build arduino from source, just download binary and copy into the container
# can't find direct url for download, tries to route through donate
COPY arduino-1.8.0-linux64.tar.xz /
RUN tar xvf arduino-1.8.0-linux64.tar.xz

# download arduino-cli
RUN wget https://github.com/arduino/arduino-cli/releases/download/0.3.6-alpha.preview/arduino-cli-0.3.6-alpha.preview-linux64.tar.bz2
RUN tar jxvf arduino-cli-0.3.6-alpha.preview-linux64.tar.bz2
RUN mv arduino-cli-0.3.6-alpha.preview-linux64 arduino-cli
#needed to work around: https://github.com/arduino/arduino-cli/issues/133
ENV USER root 

# download DUE board support
RUN /arduino-cli core update-index
RUN /arduino-cli core install arduino:sam@1.6.8

# copy these two files into arduino profile dir
RUN cp /Repetier-Firmware-4-Davinci/src/ArduinoDUE/AdditionalArduinoFiles/Arduino\ -\ 1.8.0\ -Due\ 1.6.8/Arduino15/packages/arduino/hardware/sam/1.6.8/variants/arduino_due_x/variant.cpp ~/.arduino15/packages/arduino/hardware/sam/1.6.8/variants/arduino_due_x/
RUN cp /Repetier-Firmware-4-Davinci/src/ArduinoDUE/AdditionalArduinoFiles/Arduino\ -\ 1.8.0\ -Due\ 1.6.8/Arduino15/packages/arduino/hardware/sam/1.6.8/cores/arduino/USB/USBCore.cpp ~/.arduino15/packages/arduino/hardware/sam/1.6.8/cores/arduino/USB/

# compile davinci repetier firmware
RUN /arduino-cli compile --fqbn arduino:sam:arduino_due_x /Repetier-Firmware-4-Davinci/src/ArduinoDUE/Repetier/

# to upload (to run after container image is built and you're connected to the printer)
CMD ["/arduino-cli", "upload", "-p", "/dev/ttyACM0", "--fqbn", "arduino:sam:arduino_due_x", "/Repetier-Firmware-4-Davinci/src/ArduinoDUE/Repetier/"]

# if arduino-cli doesn't work and you need the gui:
# podman run --rm -it -e DISPLAY --rm -v /tmp/.X11-unix:/tmp/.X11-unix -v /dev/dri:/dev/dri --security-opt=label=type:container_runtime_t --net=host --device=/dev/ttyACM0:rwm {imagename} bash
# reference: http://sham1.sinervo.fi/blog/x11_and_podman.html
