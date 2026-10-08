{
  description = "A fast and small file crawler in C++";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = {
    self,
    nixpkgs,
  }: let
    eachSystem = nixpkgs.lib.genAttrs nixpkgs.lib.systems.flakeExposed;
  in {
    devShells = eachSystem (
      system: let
        pkgs = nixpkgs.legacyPackages."${system}";
      in {
        default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            # Compiler and build tools
            llvmPackages_21.clang-tools
            llvmPackages_21.clang

            cmake
            gnumake
            pkg-config
          ];

          buildInputs = with pkgs;
            [
              # Debugging and profiling tools
              gdb
              valgrind

              perf
              hyperfine

              spdlog # Logging
              pcre2.dev # Regex
            ]
            ++ pkgs.lib.optional pkgs.stdenv.isLinux [];

          shellHook = ''
            # Set compiler
            export CC=${pkgs.llvmPackages_21.clang}/bin/clang
            export CXX=${pkgs.llvmPackages_21.clang}/bin/clang++
          '';
        };
      }
    );

    packages = eachSystem (
      system: let
        pkgs = nixpkgs.legacyPackages."${system}";
      in {
        default = pkgs.llvmPackages_21.stdenv.mkDerivation {
          pname = "ff";
          version = "0.0.1";
          src = pkgs.nix-gitignore.gitignoreSource [] ./.;

          cmakeBuildType = "Release";

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
          ];

          buildInputs = with pkgs; [
            spdlog
            pcre2.dev
          ];

          meta = {
            description = "ff";
            mainProgram = "ff";
          };
        };
      }
    );
  };
}
