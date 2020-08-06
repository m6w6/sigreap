# sigreap

sigreap is a simple tool to run cumbersome programs like old-style
daemons (i.e. those which fork and exec at will) under a service
manager like systemd in a more reliable way while forwarding all signals
(except STOP and KILL) to them.

It was initially thought to be used to run PHP-FPM with its
[graceful reload patch](https://github.com/php/php-src/pull/3758).

## Usage

`sigreap <program> [<args> ...]`

E.g.

`sigreap /usr/bin/php-fpm -y /etc/php/fpm.conf`

## Build

Just run `make sigreap`.

## License

sigreap is licensed under the 2-Clause-BSD license, which can be found in
the accompanying [LICENSE](./LICENSE) file.

## Contributing

All forms of contribution are welcome! Please see the bundled
[CONTRIBUTING](./CONTRIBUTING.md) note for the general principles followed.
