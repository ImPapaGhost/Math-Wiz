# Testing

## Flipper Zero

Tested on Unleashed 093e:

- `42.50*(1+18%)` returned `50.15`.
- `sqrt(3^2+4^2)` returned `5`.
- The sine graph opened; pan, zoom, and trace worked.
- The app closed and reopened.

These checks used the [Unleashed build](dist/unleashed-093/math_wiz.fap). The FAP in `main` targets official firmware 1.4.3 / API 87.1.

## Tests

```sh
./tests/run.sh
CC=clang SANITIZE=1 ./tests/run.sh
```
