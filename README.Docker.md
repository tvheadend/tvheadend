# TVHeadend in Docker
TVHeadend can be run within a Docker container. This provides isolation from
other processes by running it in a containerized environment. As this is not
and in-depth tutorial on docker, those with Docker, containers or cgroups see
[docker.com][docker].


## Building the TVHeadend image
While it is recommended to pull the image from the [GHCR][ghcr] (GitHub
Container Registry), it is certainly possible to build the image locally.

To do so, clone this repository:

$ git clone https://github.com/tvheadend/tvheadend

Then, from within the repository

```sh
docker buildx build
    --rm \
    --tag 'tvheadend:issue-123' \
    './'
```

The tag 'issue-123' in the example, is just that, an example, anything can be
used for the tag.

> _Note_: Omitting the tag, will use `latest` by default.


## Running the TVHeadend image
If the container wasn't built with the above instructions, it can be optionally
be pulled from the [GHCR][ghcr] first instead.

```sh
docker image pull ghcr.io/tvheadend/tvheadend:latest
```

> _Note_: The `latest` tag can be replaced with any desired tag, including
> `master` for the git development version.

Running the TVHeadend container is then done as follows.

```sh
docker container run \
    --interactive \
    --name 'TVHeadend_container_01' \
    --rm \
    --tty \
    'ghcr.io/tvheadend/tvheadend:issue-123' \
    --firstrun
```

> _Note_: Docker will try to pull a container it doesn't know about yet. So if
> the container was not previously built or pulled, the `run` sub command
> will try to pull it instead. Likewise, `--pull always` can be used to force
> a pull. See the docker documentation for more details.

The above will now run TVHeadend and the log output should be visible.

In the snippet above, the `--firstrun` flag was used. This flag is of course
optional. Please do read the remainder of the next chapter
[Persistently storing configuration](#Persistently-storing-configuration)
to learn more about where configuration is stored.


## Persistently storing configuration
Containers do not store files persistently, they are ephemeral by design.
Obviously, storing of configuration (or video) data is desirable and of course
is there a solution for this. There are three (probably more) potential options.

It is also important be aware that the configuration directory can be different
depending on how TVHeadend is started. If tvheadend is started, and a directory
exists in `/var/lib/tvheadend` or `/etc/tvheadend` which matches the UID of the
user executing TVHeadend, those locations will be preferred. Since the container
will always run as the user `tvheadend` and these locations are always created,
TVHeadend in the container will always use `/var/lib/tvheadend` as its
configuration directory.

Before diving more deeply into things, the current container has two default
volumes defined, `/var/lib/tvheadend` and `/var/lib/tvheadend/recordings`. These
can also be defined as named/external volumes as well as more that can be added.


### Secrets
Docker secrets are files that can be mounted in a container. They are not
ever written to disk (they live in `/run/secrets/`). While useful, not
applicable, as they can't be controlled runtime, and store one file per secret.


### Config
Very similar to Secrets, but stored on disk within the container.


### Volumes
Volumes come in a few flavors actually, but the basics are the same. A local
directory is mounted inside the container. The flavors come in the form of
where to get the source directory from.


#### Local directory
Lets say the docker container should share the configuration files with the
local user. In such case, the local `.config/hts` directory is needed within
the container, can be easily mounted with the following example. Note that
the `--volume` flag requires a absolute path.

```sh
docker container run \
    --interactive \
    --name 'TVHeadend_container_01' \
    --rm \
    --tty \
    --user "$(id -u):$(id -g)" \
    --volume '/home/user/.config/hts:/var/lib/tvheadend:rw' \
    --volume '/home/user/Videos:/var/lib/tvheadend/recordings:rw' \
    'ghcr.io/tvheadend/tvheadend:issue-123'
```

> _Note_: The `--user` flag is also used here, to ensure file-ownership does
> not change. By default the TVHeadend container runs as user `tvheadend`
> which may not have the same UID or GID as the local user. If the `id` command
> is not available, `"1000:1000"` could be used instead, where `1000` would be
> the actual UID and GID for the current user. The `rw` (read-write) or `ro`
> (read-only) flags can set the access mode, but are optional.


#### Anonymous Volumes
Docker manage volumes also internally. This is actually very common when using
docker containers, as the volumes are fully docker managed. It is even possible
to share volumes between containers, e.g. have multiple TVHeadend instances
running, with their own configuration, but sharing the recordings volume.

The TVHeadend container already creates an anonymous volume by default, so that
configuration is stored/re-used. Anonymous volumes are not 'forever persistent'
and are removed by regular cleanup actions (`docker system prune` for example).


#### Named Volumes
Docker named volumes are manually created and persistently stored. For long
term use (using a server for example), they are the preferred way of handling
data. Docker compose can create them automatically (more on that later) but
generally, a volume is created beforehand as such.

$ docker volume create 'hts_config'

and mounted as:

```sh
docker container run \
    --interactive \
    --name 'TVHeadend_container_01' \
    --rm \
    --tty \
    --user "$(id -u):$(id -g)" \
    --volume 'hts_config:/var/lib/tvheadend:rw' \
    --volume '/home/user/Videos:/var/lib/tvheadend/recordings:rw' \
    'ghcr.io/tvheadend/tvheadend:issue-123'
```


### Additional files
It is possible to use the volumes concept to add additional files to an
dockerized setup. For example if there is a volume holding various picons,
which are created and managed/updated through a different container. The volume
can be simply added in the same way.


```sh
docker container run \
    --interactive \
    --name 'TVHeadend_container_01' \
    --rm \
    --tty \
    --user "$(id -u):$(id -g)" \
    --volume 'hts_config:/var/lib/tvheadend:rw' \
    --volume '/home/user/Videos:/var/lib/tvheadend/recordings:rw' \
    --volume 'picons_volume:/usr/share/picons' \
    'ghcr.io/tvheadend/tvheadend:issue-123'
```

> _Note_: The same can be done for directories holding binaries or scripts
> but beware that this may not always work as expected. For example if a
> python script is added, and requires the python interpreter, which is not
> available in the TVHeadend container, the binary will be there, but it cannot
> run. The same is true for an executable, if the libraries it depends on are
> not there, it will fail to run. If in doubt, entering the container by
> appending `/bin/sh` and running the binary or `ldd`ing may give a clue.


### Environment variables
TVHeadend can also consume environment variables for additional configuration.
Most notably being the timezone. Environment variables can be easily added from
the commandline.

```sh
docker container run \
    --env 'TZ="Etc/UTC"' \
    --interactive \
    --name 'TVHeadend_container_01' \
    --rm \
    --tty \
    --user "$(id -u):$(id -g)" \
    --volume 'hts_config:/var/lib/tvheadend:rw' \
    --volume '/home/user/Videos:/var/lib/tvheadend/recordings:rw' \
    --volume 'picons_volume:/usr/share/picons' \
    'ghcr.io/tvheadend/tvheadend:issue-123'
```


## Configuring devices
TVHeadend gets most of its use via external devices, DVB tuners and the like.
The following section is completely optional however if no devices need to be
mapped to the container however.

There are a some caveats depending on static or dynamic devices.

In both cases, the device needs to exist however when starting docker and
the permissions to access the device need to be correct. The TVHeadend
container is by default part of the `video`, `audio` and `usb` groups,
which seems to use the same UID on at least Gentoo and Alpine, but Arch has
different GID's.

> _Note_: If there is no shared GID, and no desire to change the GID of the
> host, it is also possible to give **HIGHLY UNRECOMMENDED** 666 permissions to
> the device.


### Static devices
For static devices, that are not added or removed while the container is
running, this is easy enough with the add the `--device` flag. Assuming
TVHeadend is to take care of all devices, the entire dvb directory can be
shared.

```sh
docker container run \
    --env 'TZ="Etc/UTC"' \
    --device '/dev/dvb' \
    --interactive \
    --name 'TVHeadend_container_01' \
    --rm \
    --tty \
    --user "$(id -u):$(id -g)" \
    --volume 'hts_config:/var/lib/tvheadend:rw' \
    --volume '/home/user/Videos:/var/lib/tvheadend/recordings:rw' \
    --volume 'picons_volume:/var/lib/picons' \
    'ghcr.io/tvheadend/tvheadend:issue-123'
```


### Dynamic devices
Dynamic devices are a lot harder to deal with, due to their dynamic nature.
For DVB devices, that only create and remove themselves from `/dev/dvb` this
should work. Devices however that get exposed by their exact path, things
are less easy.

One thing that seems to work quite well, is to create a specific udev rule,
that creates a symlink the device seems to work well. The following example
exposes a USB serial converter as `/dev/myserialdevice`.

```
ACTION=="add",
SUBSYSTEM=="tty",
ATTRS{idVendor}=="1236",
ATTRS{idProduct}=="5678",
ATTRS{serial}=="12345678",
MODE="660",
TAG+="uaccess",
SYMLINK+="myserialdevice"
```

> _Note_: Worse comes to worst, the entire `/dev` directory could be device
> mounted or the `--privileged` flag, but are **HIGHLY UNRECOMMENDED** from
> a security (and isolation) perspective.
> Also `udevadm info -q all -n '/dev/mydevice'` can be used to inspect a device
> and also show any currently installed symlinks.
> Finally, if permission access is not working, the `MODE` could be set to an
> insecure `666`.

### Other devices
The above section spoke mostly of adding DVB devices to the container, but
other devices can be added as well, for example `/dev/dri` to map a GPU for
encoding acceleration, assuming the needed tools are available to do so.

> _Tip_: It is actually better to have a dedicated container running conversion
> and share storage locations via volumes instead.

## Network access
TVHeadend is a network connected device, but by default, docker will not map
any ports that a service listen to the host. Full network isolation. As such
network ports need to be published to the host. The `--publish` flag does so.
In the example below, a range of ports gets published to all devices via the
IP address `0.0.0.0`. This can be restricted to a specific interface via its
specific IP address.

```sh
docker container run \
    --device '/dev/dvb' \
    --env 'TZ="Etc/UTC"' \
    --interactive \
    --name 'TVHeadend_container_01' \
    --publish "0.0.0.0:9981-9982:9981-9982/tcp" \
    --rm \
    --tty \
    --user "$(id -u):$(id -g)" \
    --volume 'hts_config:/var/lib/tvheadend:rw' \
    --volume '/home/user/Videos:/var/lib/tvheadend/recordings:rw' \
    --volume 'picons_volume:/var/lib/picons' \
    'ghcr.io/tvheadend/tvheadend:issue-123'
```


> _Note_: A proper docker setup isolates container's networking completely with
> unique interfaces per container. The default `run` command as used throughout
> this guide however, uses the default docker network, which means that
> individual containers are actually on the same network internally. Think of
> it as a networking router controlled by docker, where all containers are
> plugged into. A proper setup can be done with the normal `docker run`, but
> is out of scope, as it is better to use `docker compose` for that. More on
> this later.


### CAClient access
Using a CAClient over the network, requires that both containers are on the
same network, as they have to be to reach each other. With the default
network setup of `docker run`, this shouldn't be an issue generally.


### Firewall
There currently exists a bug with docker, in that a default firewall policy of
`DROP` on the `INPUT` chain breaks certain container to container traffic.

While it is possible to craft `iptables` rules to fix this, the dynamic nature
of everything makes this very tricky in general. The only work around is
to set the default policy to `ACCEPT` and ensure the chain `DROPS` all packets.

Assuming `ppp0` is the internet connection that is a monitored interface and
everything else is `okay`. On an empty/firewall;

$ iptables -A -i ppp0 -j DROP
$ iptables -P INPUT DROP


## Troubleshooting
To trouble shoot device pass through, networking, or other permission related
ales, it is possible to start the container in privileged mode, connect directly
to the host network and run TVHeadend as root.

All are **NOT** recommended for normal use, but can help isolate issues.

$ docker container run --privileged --user 'root:root' --network 'host' ...

> _Tip_: After running the container as root, make sure to change all
> ownership back to `tvheadend:tvheadend`, most likely the configuration files.
> This is best done from within the container using
>
> $ chmod -R 'tvheadend:tvheadend' '/home/tvheadend'
>


## Compose
Docker compose can be used to define whole cluster of services. While this entry
will not be an expert piece on how to properly define compose services and setup
whole service clusters, here is an example that puts all of the above in a
single file. It is here for reference only and users are expected to know what
they are doing when using this.

```yaml
networks:
  tvheadend: {}
  oscam: {}

volumes:
  tvheadend:
  oscam:

services:
  oscam:
    image: docker.io/olliver/oscam:latest # TODO
    cap_drop:
      - all
    ulimits:
      nproc: 64
      nofile:
        soft: 1024
        hard: 65535
    devices:
      - /dev/cardreader
    env_file:
      - common.env
    volumes:
      - oscam:/etc/oscam:rw
    networks:
      - oscam
    expose:
      - "1988/udp"
      - "15000/tcp"
    ports:
      - "8888:8888/tcp"
    command: -d 64
    restart: unless-stopped
    healthcheck:
      test: wget localhost:8888 2>&1 | grep -q 'connected'


services:
  tvheadend:
    image: ghcr.io/tvheadend/tvheadend:latest
    cap_drop:
      - all
    ulimits:
      nproc: 256
      nofile:
        soft: 8192
        hard: 65535
    devices:
      - /dev/dvb/
    environment:
      - TZ: 'Europe/Amsterdam'
    volumes:
      - tvheadend:/var/lib/tvheadend:rw
      - /export/recordings:/var/lib/tvheadend/recordings:rw
    networks:
      - tvheadend
      - oscam
    ports:
      - "9981-9982:9981-9982/tcp"
    command: --config '/var/lib/tvheadend' --nosatip
    restart: unless-stopped
    healthcheck:
      test: wget -O - -q 'http://localhost:9981/ping' | grep -q 'PONG'
```

> _Tip_: The `ulimits` are optional but recommended. Oscam uses a `common.env`
> file holding the Timezone, TVHeadend uses a variable, both achieve the same.

> _Note_: TVHeadend automatically detects the location for storage, the above
> is just an example, if the location is changed here however, make sure to
> update the configuration in the UI to match.


## Developing using a container
While it is certainly easy and possible to develop using `docker image build`,
it is quite slow, as some setup and teardown work needs to be done before
compilation can be done, also already compiled objects get discarded.

Instead, the `builder` container, an intermediate step, can be used instead
and the source directory volume mounted herein.

```sh
docker image build
    --rm \
    --tag 'tvheadend:builder' \
    --target 'builder' \
    './'
```

> _Note_: Because the way the builder is set up, it will build everything
> once regardless. This is to keep the current `Containerfile` simple.
> Other then the cost of some time, it is harmless.

With the built `builder` image, the container can be entered, and make can be
issued as normal.

```sh
docker container run \
    --interactive \
    --rm \
    --tty \
    --user "$(id -u):$(id -g)" \
    --volume "$(pwd):/workdir" \
    --workdir '/workdir' \
    'tvheadend:builder' '/bin/sh'
./configure
make
```

> _Tip_: It might be tempting to short-circuit the command `/bin/sh` by
> replacing it with `make`. However it will still be slower due to the
> container creation/teardown, but otherwise functionally identical.

> _Note_: As the current working dir is volume mounted into the container,
> all changes done by the container will be made on the regular source code
> directory. As such, exiting and re-starting the container won't remove any
> intermediate object files etc. As the build-container runs normally runs as
> root, the `--user` flag has to be set as otherwise files created will be
> owned by root when exiting the container.


[docker]: https://www.docker.com
[ghcr]: https://github.com/tvheadend/tvheadend/pkgs/container/tvheadend
