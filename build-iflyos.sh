#!/bin/bash
set -e

POSITIONAL=()
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -c|--client)
    CLIENT_ID="$2"
    shift # past argument
    shift # past value
    ;;
    -s|--secret)
    CLIENT_SECRET="$2"
    shift # past argument
    shift # past value
    ;;
    -d|--device)
    DEVICE_ID="$2"
    shift # past argument
    shift # past value
    ;;
    -p|--platform)
    PLATFORM="$2"
    shift # past argument
    shift # past value
    ;;
    -r|--product)
    PRODUCT="$2"
    shift
    shift
    ;;
    -m|--microphone)
    MIC="$2"
    shift
    shift
    ;;
    -o|--outputdir)
    OUTPUT="$2"
    shift
    shift
    ;;
    *)    # unknown option
    POSITIONAL+=("$1") # save it in an array for later
    shift # past argument
    ;;
esac
done

set -- "${POSITIONAL[@]}" # restore positional parameters

set -e

if [ -z "$PLATFORM" ]
then
  echo "no platform arg"
  exit 1
fi

if [ $PLATFORM == "rpi" ]
then
  PRODUCT="rpi"
fi

if [ -z "$PRODUCT" ]
then
  echo "no product arg"
  exit 1
fi

echo "use platform $PLATFORM"

if [ -z "$CLIENT_ID" ]
then
  echo "no client id set"
  exit 1
fi

echo "use client id $CLIENT_ID"

if [ -z "CLIENT_SECRET" ]
then
  echo "no client secret set"
  exit 1
fi

echo "use client secret $CLIENT_SECRET"

if [ -z "$DEVICE_ID" ]
then
  echo "device id not set, will use MAC address"
else
  echo "use device id $DEVICE_ID"
fi

if [ -z "$MIC" ]
then
  echo "no mic number set(-m)"
else
  echo "use microphone number $MIC"
fi

if [ ! -d jsruntime ]
then
  echo "no jsruntime folder"
  exit 1
fi

if [ ! -d mediaplayer ]
then
  echo "no mediaplayer folder"
  exit 1
fi

if [ ! -d smartSpeakerApp ]
then
  echo "no smartSpeakerApp folder"
  exit 1
fi

if [[ ! "$OUTPUT" == /* ]]
then
    OUTPUT=$PWD/$OUTPUT
fi

srcdir=$PWD
toolchain="$srcdir/jsruntime/iflyosBoards/$PLATFORM/toolchain.txt"
buildtype=Release
arch=arm

if [ $PLATFORM == "linuxHost" ]
then
buildtype=Debug
arch=x86_64
fi

if [ $PLATFORM == "mtk8516" ]
then
export PATH=/etc/iflyosBoards/mtk8516/sysroots/x86_64-oesdk-linux/usr/bin:/etc/iflyosBoards/mtk8516/sysroots/x86_64-oesdk-linux/usr/sbin:/etc/iflyosBoards/mtk8516/sysroots/x86_64-oesdk-linux/bin:/etc/iflyosBoards/mtk8516/compiler/sysroots/x86_64-oesdk-linux/sbin:/etc/iflyosBoards/mtk8516/compiler/sysroots/x86_64-oesdk-linux/usr/bin/../x86_64-oesdk-linux/bin:/etc/iflyosBoards/mtk8516/compiler/sysroots/x86_64-oesdk-linux/usr/bin/aarch64-poky-linux:/etc/iflyosBoards/mtk8516/compiler/sysroots/x86_64-oesdk-linux/usr/bin/aarch64-poky-linux-musl:$PATH
arch=aarch64
fi

if [ ! -f $toolchain ]
then
  echo "no $toolchain"
  exit 1
fi

outdir=$OUTPUT/$PLATFORM/build
installdir=$OUTPUT/$PLATFORM/install
echo $outdir
echo $installdir
mkdir -p $outdir/jsruntime
mkdir -p $outdir/mediaplayer
mkdir -p $installdir/run
mkdir -p $installdir/smartSpeakerApp

if [ $PLATFORM == "rpi" ]
then
echo "building libwpa_client to $outdir/wpa_ctrl"
mkdir -p $outdir/wpa_ctrl
cd $outdir/wpa_ctrl
cmake -DCMAKE_BUILD_TYPE=$buildtype -DCMAKE_INSTALL_PREFIX=$installdir $srcdir/wpa_ctrl
make -j4
make install
cp -r $srcdir/snowboy $installdir/
fi

echo "building jsruntime to $outdir/jsruntime"
cd $outdir/jsruntime
echo "-DMIC${MIC}=ON" -DIFLYOS_PRODUCT=${PRODUCT}

cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=$toolchain -DTARGET_ARCH=$arch \
"-DMIC_${MIC}=ON" \
-DIFLYOS_PRODUCT=${PRODUCT} \
-DTARGET_BOARD=None \
-DENABLE_LTO=OFF \
-DENABLE_MODULE_NAPI=OFF \
-DENABLE_SNAPSHOT=OFF \
-DEXPOSE_GC=OFF \
-DBUILD_LIB_ONLY=OFF \
-DCREATE_SHARED_LIB=ON \
-DFEATURE_MEM_STATS=OFF \
-DEXTERNAL_MODULES='' \
-DFEATURE_PROFILE=es2015-all \
-DMEM_HEAP_SIZE_KB=4096 \
-DEXTRA_JERRY_CMAKE_PARAMS='-DFEATURE_CPOINTER_32_BIT=ON' \
-DFEATURE_JS_BACKTRACE=ON \
-DEXTERNAL_LIBS='' \
-DEXTERNAL_COMPILE_FLAGS='' \
-DEXTERNAL_LINKER_FLAGS='' \
-DEXTERNAL_INCLUDE_DIR='' $srcdir/jsruntime

make -j4
make DESTDIR=$installdir install

if [ ! $PLATFORM == "mtk8516" ]
then
echo "building mediaplayer to $outdir/mediaplayer"
cd $outdir/mediaplayer
cmake -DCMAKE_BUILD_TYPE=$buildtype -DCMAKE_TOOLCHAIN_FILE=$toolchain -DCMAKE_INSTALL_PREFIX=$installdir $srcdir/mediaplayer
make -j4
make install
find $installdir -type d -name "include" | xargs rm -rf
rm -rf $installdir/usr/local/lib/pkgconfig
fi

echo "=========successfuly built at $installdir============="
cp -rf $srcdir/snowboy  $installdir/snowboy
cp $srcdir/jsruntime/iflyosBoards/$PLATFORM/evsCtrl.sh $installdir/bin
cp $srcdir/jsruntime/iflyosBoards/$PLATFORM/iFLYOS.json $installdir/
cp -rf $srcdir/smartSpeakerApp/src/*.js $installdir/smartSpeakerApp/
cp -rf $srcdir/smartSpeakerApp/sound_effect $installdir/sound_effect
mkdir -p $installdir/smartSpeakerApp/platforms
cp -rf $srcdir/smartSpeakerApp/src/platforms/`ls $srcdir/smartSpeakerApp/src/platforms | grep -i ${PLATFORM}` $installdir/smartSpeakerApp/platforms

sed -i "s:__PLAYER_BIN__:$installdir/bin/RMediaPlayer:g" $installdir/iFLYOS.json
sed -i "s/__CLIENT_ID__/$CLIENT_ID/g" $installdir/iFLYOS.json
sed -i "s/__DEVICE_ID__/$DEVICE_ID/g" $installdir/iFLYOS.json
sed -i "s/__CLIENT_SECRET__/$CLIENT_SECRET/g" $installdir/iFLYOS.json
sed -i "s:__SNOWBOY_RES__:$installdir/snowboy/common.res:g" $installdir/iFLYOS.json
sed -i "s:__SNOWBOY_MODEL__:$installdir/snowboy/snowboy.umdl:g" $installdir/iFLYOS.json
sed -i "s:__DB_PATH__:$installdir/run:g" $installdir/iFLYOS.json
sed -i "s:__SOUND_DIR__:$installdir/sound_effect:g" $installdir/iFLYOS.json
