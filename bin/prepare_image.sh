#
# Prepare a test image mounted on mnt
#
#
# Create a TTY device and copy some files from the userspace
# directory into it, then chown
#
sudo mkdir -p mnt/dev
sudo mknod -m 666 mnt/dev/tty c 2 0
sudo mkdir -p mnt/bin
sudo mkdir -p mnt/tmp
sudo cp ../userspace/cli ../userspace/init ../userspace/args mnt/bin
sudo mkdir -p mnt/tests
sudo cp  ../userspace/tests/testjc ../userspace/tests/testwait ../userspace/tests/testfiles ../userspace/tests/testsignals ../userspace/tests/testhello mnt/tests
sudo cp ../userspace/tests/testpipes ../userspace/tests/testfork ../userspace/tests/testmisc ../userspace/tests/testtty ../userspace/tests/testatexit mnt/tests
sudo cp ../userspace/tests/testall ../userspace/tests/testnet ../userspace/tests/testtabs mnt/tests
if [ -d "import" ]
then
  sudo cp -r -v ./import/* ./mnt/
fi
sudo cp ../userspace/uname mnt/bin/ctOSuname
echo "Hello" > /tmp/hello
sudo cp /tmp/hello mnt
sudo chown -R $ctOSUser mnt/*
