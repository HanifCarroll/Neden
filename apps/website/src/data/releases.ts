/** Only add a download URL after the packaged artifact has passed its release checks. */
export interface PlatformRelease {
  name: string
  id: string
  requirement: string
  download?: string
  status: string
}
const downloadBase = "https://github.com/HanifCarroll/Neden/releases/download/v0.2.0-preview.1"

export const releases: PlatformRelease[] = [
  {
    name: "macOS",
    id: "macos",
    requirement: "macOS 13+ · Apple silicon",
    download: `${downloadBase}/Neden-macos-arm64-preview.dmg`,
    status: "Preview · not notarized",
  },
  {
    name: "Windows",
    id: "windows",
    requirement: "Windows 10 / 11 · 64-bit",
    download: `${downloadBase}/Neden-windows-x86_64-preview-setup.exe`,
    status: "Experimental preview · unsigned",
  },
  {
    name: "Linux",
    id: "linux",
    requirement: "Ubuntu 24.04 · 64-bit",
    download: `${downloadBase}/Neden-linux-x86_64-preview.deb`,
    status: "Experimental preview · .deb",
  },
]
export const releasesUrl = "https://github.com/HanifCarroll/Neden/releases"
