{
  description = "chest";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    catch2 = {
      url = "github:catchorg/Catch2/v3.4.0";
      flake = false;
    };
  };

  outputs = {
    self,
    nixpkgs,
    catch2,
  }: let
    systems = ["x86_64-linux"];
    forAllSystems = nixpkgs.lib.genAttrs systems;
  in {
    packages = forAllSystems (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
        stdenv = pkgs.clangStdenv;
        base_attrs = {
          preConfigure = ''
            cp -r ${catch2} ./catch2
            substituteInPlace CMakeLists.txt \
              --replace "GIT_REPOSITORY https://github.com/catchorg/Catch2.git" "SOURCE_DIR ${catch2}"
          '';

          name = "chest";
          src = ./src;
          # Provides libatomic
          buildInputs = [pkgs.gcc.cc];
          nativeBuildInputs = [pkgs.cmake pkgs.lld];

          shellHook = ''
            export CLANGD_FLAGS="--query-driver=${pkgs.clang}/bin/clang++"
          '';
        };
      in {
        default = stdenv.mkDerivation (base_attrs
          // {
            cmakeFlags = ["-DCMAKE_CXX_FLAGS='-march=haswell'"];
          });
        base = stdenv.mkDerivation base_attrs;
      }
    );
  };
}
