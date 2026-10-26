/** Only add a download URL after the packaged artifact has passed its release checks. */
export interface PlatformRelease {
  name: string
  id: string
  requirement: string
  download?: string
  status: string
}
export const releases: PlatformRelease[] = [
  {
    name: "macOS",
    id: "macos",
    requirement: "Apple silicon",
    status: "Installer validation in progress",
  },
  {
    name: "Windows",
    id: "windows",
    requirement: "64-bit PC",
    status: "Installer validation in progress",
  },
  {
    name: "Linux",
    id: "linux",
    requirement: "64-bit PC",
    status: "Installer validation in progress",
  },
]
export const releasesUrl = "https://github.com/HanifCarroll/Neden/releases"
