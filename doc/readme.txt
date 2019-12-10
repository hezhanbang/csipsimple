ubuntu-16.04-x86_64

sudo apt-get update -y
sudo apt-get install -y swig2.0 quilt

sudo apt-get install -y vim libtool gcc automake autoconf autopoint make gettext git bzr subversion cmake cvs yasm zip p7zip bzip2 aptitude

wget https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/csipsimple/source-archive.zip
unzip source-archive.zip

https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/bkvoice/$(silk_remote_folder).zip

	@wget --no-check-certificate $(silk_repo); \
	unzip -d $< sources -q $(silk_remote_folder).zip
	@rm $(silk_remote_folder).zip
	@#mv sources/SILK_SDK_SRC_$(silk_version)/* sources/

https://github.com/jacklicn/webm.libvpx.git

apt-get install -y default-jdk ant

#要下载的第三方软件
mkdir -p /home/bang/third
cd /home/bang/third
git clone https://android.googlesource.com/platform/external/aac

# ndk
wget -O /home/bang/third/android-ndk-r10e-linux-x86_64.zip https://dl.google.com/android/repository/android-ndk-r10e-linux-x86_64.zip?hl=zh_cn
unzip android-ndk-r10e-linux-x86_64.zip
vim ~/.bashrc
export ANDROID_NDK=/home/bang/third/android-ndk-r10e
export PATH=$ANDROID_NDK:$PATH
echo $ANDROID_NDK
whereis ndk-build

# android sdk
wget https://dl.google.com/android/repository/sdk-tools-linux-4333796.zip
wget http://dl.google.com/android/adt/adt-bundle-linux-x86_64-20131030.zip
vim ~/.bashrc
export ANDROID_SDK=/home/bang/third/adt-bundle-linux-x86_64-20131030/sdk
export PATH=$ANDROID_SDK/tools/:$ANDROID_SDK/platform-tools:$PATH
echo $ANDROID_SDK
whereis android

sudo apt-get install lib32stdc++6 lib32z1

cd /home/bang/csipsimple/trunk/CSipSimple
	make

cd /home/bang/csipsimple/trunk/ActionBarSherlock
	android list targets
	android update project --target 1 -p ./

cd /home/bang/csipsimple/trunk/CSipSimple
	android update project -p ./
	ant release

# 向apk添加数字签名
cd /home/bang/csipsimple/signApk/
./signApk.sh

 打开ecplise，创建虚拟机RAM:900 MV-Heap:32, 并开启虚拟机。
/home/bang/third/adt-bundle-linux-x86_64-20131030/eclipse/eclipse

# 等虚拟机开机完成后，把CSipSimple安装上去。
./install2AndroidVirtualDevice.sh

# 重新编译
cd /home/bang/csipsimple/trunk/CSipSimple
	ant clean

/src/com/csipsimple/api/SipConfigManager.java:68:    public static final String ECHO_CANCELLATION_TAIL = "echo_cancellation_tail";
./src/com/csipsimple/api/SipConfigManager.java:114:    public static final String ECHO_CANCELLATION = "echo_cancellation";
./src/com/csipsimple/ui/incall/InCallMediaControl.java:84:		echoCancellation = (CheckBox) findViewById(R.id.echo_cancellation);
./src/com/csipsimple/ui/incall/InCallMediaControl.java:363:				if (bId == R.id.echo_cancellation) {

# 把安卓上的日志文件复制到PC
adb pull "/storage/sdcard0/CSipSimple/logs/pjsiplogs_19-12-05_165038.txt" .

# csipsimple的安装目录
/data/app/com.csipsimple-1/
