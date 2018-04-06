setuid
======

This tool was born because, in OpenShift, containers run as essentially arbitrary user id's. The way some
NFS implementations squash root to nobody.nobody, if a containers umask is set to remove group writability
on newly created files, you wind up in a situation where root cannot clean out NFS mounts. This is typically
seen in the persistent volume recycler in OpenShift.

There may be other uses of this tool.
