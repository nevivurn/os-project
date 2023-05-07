{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/release-22.11";
  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            bison
            flex
            ccache
            qemu
            gdb
            pkgsCross.aarch64-multiplatform.stdenv.cc
          ] ++ pkgs.linux_4_19.nativeBuildInputs;
        };
      }
    );
}
