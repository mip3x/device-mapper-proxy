# device-mapper-proxy

# **Installation**

## **Ubuntu**

### **Install build dependencies**  
```bash
sudo apt update
sudo apt install -y build-essential kmod 
```

## **Get sources**
```bash
git clone https://mip3x/device-mapper-proxy.git
cd device-mapper-proxy
```

# **Build module**
```bash
make
```

# **Load module**
```bash
sudo insmod dmp.ko
sudo dmesg | tail -n 5 | grep dmp
```
You should now see line "dmp: module loaded". It means module is loaded properly. 

# **Test module**

## **Verify that dm-target is registered**
```bash
dmsetup targets | grep dmp
```
You should now see line "dmp v$version".
Note: `$version` -- version of program.

## **Create backend and proxy devices**
```bash
sudo dmsetup create $backend_name --table "0 $size zero"
sudo dmsetup create $proxy_name --table "0 $size dmp /dev/mapper/$backend_name"
```
`zero` could be replaced with another backend device.
Note: `$size` -- random device size.

## **Verify creation fact**
```bash
ls -l /dev/mapper/*
```
You should now see lines with `$backend_name` and `$proxy_name`. It means devices created properly.

## **Generate I/O requests**
```bash
dd if=/dev/zero of=/dev/mapper/$proxy_name bs=4k count=10
dd if=/dev/mapper/$proxy_name of=/dev/null bs=4k count=5
```

## **View statistics**
```bash
cat /sys/module/dmp/stat/volumes
```
You should now see statistics output:
```
read:
    reqs: 5
    avg size: 4096
write:
    reqs: 10
    avg size: 4096
total:
    reqs: 15
    avg size: 4096
```
In fact, the number of records is unlikely to match the number provided when calling `dd`, since write and read requests are also performed when creating the device: I did not reset the counters after the end of the initialization phase.

## **Cleanup**
```bash
sudo dmsetup remove $proxy_name
sudo dmsetup remove $backend_name
sudo rmmod dmp
sudo dmesg | tail -n 5 | grep dmp
```
You should now see line "dmp: module unloaded". It means module is unloaded properly. 
