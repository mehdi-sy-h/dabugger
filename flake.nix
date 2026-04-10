{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs =
    { self, nixpkgs }@inputs:
    let
      system = "x86_64-linux";
      pkgs = import inputs.nixpkgs { inherit system; };
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          clang-tools
          cmake
          gdb
          dwarfdump
        ];
      };
      packages.${system} =
        let
          package = {
            pname = "dabugger";
            version = "0.1.0";
            src = ./.;
            nativeBuildInputs = with pkgs; [ cmake ];
          };
        in
        {
          default = pkgs.clangStdenv.mkDerivation package;
          debug = pkgs.clangStdenv.mkDerivation (
            package
            // {
              cmakeBuildType = "Debug";
              dontStrip = true;
            }
          );
        };
    };
}
