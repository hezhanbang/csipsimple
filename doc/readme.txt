ubuntu-16.04-x86_64

sudo apt-get update -y
sudo apt-get install -y swig2.0 quilt

sudo apt-get install -y vim libtool gcc automake autoconf autopoint make gettext git bzr subversion cmake cvs yasm zip p7zip bzip2 aptitude

wget https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/csipsimple/source-archive.zip
unzip source-archive.zip

wget -O ~/android-ndk-r10e-linux-x86_64.zip https://dl.google.com/android/repository/android-ndk-r10e-linux-x86_64.zip?hl=zh_cn
unzip android-ndk-r10e-linux-x86_64.zip

vim ~/.bashrc
export ANDROID_NDK=/root/android-ndk-r10e
export PATH=$ANDROID_NDK:$PATH
echo $ANDROID_NDK

https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/bkvoice/$(silk_remote_folder).zip

	@wget --no-check-certificate $(silk_repo); \
	unzip -d $< sources -q $(silk_remote_folder).zip
	@rm $(silk_remote_folder).zip
	@#mv sources/SILK_SDK_SRC_$(silk_version)/* sources/

https://github.com/jacklicn/webm.libvpx.git


apt-get install -y default-jdk
wget https://dl.google.com/android/repository/sdk-tools-linux-4333796.zip
wget http://dl.google.com/android/adt/adt-bundle-linux-x86_64-20131030.zip
vim ~/.bashrc
export ANDROID_SDK=/root/adt-bundle-linux-x86_64-20131030/sdk
export PATH=$ANDROID_SDK/tools/:$ANDROID_SDK/platform-tools:$PATH
echo $ANDROID_SDK

sudo apt-get install lib32stdc++6 lib32z1
