import React from "react";
import ReactDOM from "react-dom/client";
import "./index.scss";

const CURRENT_BUILD = process.env.REACT_APP_BUILD_VERSION;

const LINKS = {
  docs: "https://docs.codeperfect95.com",
  changelog: "https://docs.codeperfect95.com/changelog",
  github: "https://github.com/codeperfect95/codeperfect",
};

const DOWNLOADS = [
  ["mac-x64", "macOS Intel"],
  ["mac-arm", "macOS M1"],
];

const HEADER_LINKS = [
  [LINKS.docs, "Docs"],
  [LINKS.changelog, "Changelog"],
  [LINKS.github, "Github"],
];

ReactDOM.createRoot(document.getElementById("root")).render(
  <React.StrictMode>
    <div className="flex flex-col gap-y-10 max-w-[550px] mx-auto p-4 md:p-8">
      <div>
        <span className="inline-flex items-center">
          <img
            alt="logo"
            className="w-auto h-8 inline-block mr-3"
            src={
              process.env.REACT_APP_CPENV === "development"
                ? process.env.PUBLIC_URL + "/logo.png"
                : "/logo.png"
            }
          />
          <span className="text-lg font-bold text-black">CodePerfect</span>
        </span>
        <div className="flex items-center gap-x-4 mt-1">
          {HEADER_LINKS.map(([url, label]) => (
            <a key={url} className="font-semibold" href={url}>
              {label}
            </a>
          ))}
        </div>
      </div>

      <div>
        <p className="font-bold text-2xl text-black">
          A fast, lightweight <span className="whitespace-nowrap">Go IDE</span>
        </p>
        <p>
          CodePerfect was an experiment to build a faster IDE. It eschews the
          modern tech stack and is instead written from scratch in C/C++/OpenGL
          like a video game.
        </p>
        <p>
          It starts instantly, runs at 144 FPS, has near-zero latency, and comes
          with native, full-featured code intelligence and integrated debugging
          with Delve. See more of the features <a href={LINKS.docs}>here</a>.
        </p>
        <p>
          It's no longer actively developed, but is now{" "}
          <a href={LINKS.github}>open source</a> and available for free.
        </p>
        <ul>
          {DOWNLOADS.map(([platform, label]) => (
            <li key={platform}>
              <a
                href={`https://codeperfect95.s3.us-east-2.amazonaws.com/app/${platform}-${CURRENT_BUILD}.zip`}
              >
                Download build {CURRENT_BUILD} &mdash; {label}
              </a>
            </li>
          ))}
        </ul>
      </div>

      <div className="text-neutral-400 text-sm">
        &copy; {new Date().getFullYear()} CodePerfect
      </div>
    </div>
  </React.StrictMode>
);
