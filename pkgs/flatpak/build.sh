#!/bin/bash

rm -f com.github.rafostar.Clapper.flatpak
rm -rf build; mkdir build
rm -rf repo; mkdir repo

flatpak-builder --ccache --force-clean --default-branch=test build com.github.rafostar.Clapper.yml --repo=repo
