import produce from "immer";
import React from "react";
import cx from "classnames";
// import _ from "lodash";

import { Helmet } from "react-helmet";
import { CopyToClipboard } from "react-copy-to-clipboard";
import {
  BrowserRouter as Router,
  Link,
  Route,
  Switch,
  useLocation,
  Redirect,
  useHistory,
  useParams,
} from "react-router-dom";

import { AiOutlineCheck, AiOutlineClose, AiFillCode } from "react-icons/ai";
import { BsArrowRight } from "react-icons/bs";
import { FaApple, FaRegClipboard } from "react-icons/fa";
import { FcCheckmark } from "react-icons/fc";
import { HiArrowNarrowLeft, HiArrowNarrowRight } from "react-icons/hi";

import "./index.css";
import gpuImage from "./gpu.svg";
import vimSvg from "./vim.svg";
import codeSvg from "./code.svg";
import workflowSvg from "./workflow.svg";
import logoImage from "./logo.png";

import animFrames from "./anim-code-intelligence/data.json";
import animSpritesheet from "./anim-code-intelligence/spritesheet.png";

import animVimFrames from "./anim-vim2/data.json";
import animVimSpritesheet from "./anim-vim2/spritesheet.png";

import animWorkflowFrames from "./anim-workflow2/data.json";
import animWorkflowSpritesheet from "./anim-workflow2/spritesheet.png";

const SUPPORT_EMAIL = "support@codeperfect95.com";

let API_BASE = "https://api.codeperfect95.com";
if (process.env.NODE_ENV === "development") {
  API_BASE = "http://localhost:8080";
}

function A({ children, ...props }) {
  return (
    <a target="_blank" rel="noreferrer" {...props}>
      {children}
    </a>
  );
}

function WallOfText({ width, children, className, ...props }) {
  // for tailwindcss shake:
  // md:max-w-0 md:max-w-none md:max-w-xs md:max-w-sm md:max-w-md md:max-w-lg
  // md:max-w-xl md:max-w-2xl md:max-w-3xl md:max-w-4xl md:max-w-5xl md:max-w-6xl
  // md:max-w-7xl md:max-w-full md:max-w-min md:max-w-max md:max-w-fit md:max-w-prose
  // md:max-w-screen-sm md:max-w-screen-md md:max-w-screen-lg md:max-w-screen-xl
  // md:max-w-screen-2xl

  return (
    <div
      className={cx(
        "wall-of-text p-4 my-4 md:my-16 md:p-12 leading-normal md:mx-auto",
        `md:max-w-${width || "3xl"}`,
        className
      )}
      {...props}
    >
      {children}
    </div>
  );
}

function Title({ children, ...props }) {
  return (
    <h2 className="text-2xl font-bold text-gray-700" {...props}>
      {children}
    </h2>
  );
}

function Icon({ icon, ...props }) {
  const C = icon;
  return (
    <span className="relative -top-0.5">
      <C {...props} />
    </span>
  );
}

function Feature({ title, icon, children, selected, onClick, ...props }) {
  return (
    <button
      className={cx(
        "block w-full text-center group",
        "sm:text-left",
        "lg:pl-3 lg:text-left lg:border-l-4",
        selected
          ? "lg:border-gray-700 text-black hover:text-black"
          : "lg:border-gray-200 text-gray-500 hover:text-gray-600"
      )}
      onClick={onClick}
      {...props}
    >
      <div className="flex justify-center sm:justify-start">
        <img
          alt="ide"
          src={icon}
          className={cx(
            "opacity-50 text-center w-10 filter grayscale",
            "group-hover:opacity-60",
            selected && "opacity-100 group-hover:opacity-100"
          )}
        />
      </div>
      <div className="text-sm font-bold my-2">{title}</div>
      <div className="hidden sm:block">{children}</div>
    </button>
  );
}

const BIG_TABLE_OF_FEATURES = {
  code: {
    image: animSpritesheet,
    frames: animFrames,
    skip: 1500,
    speed: 1,
  },
  vim: {
    image: animVimSpritesheet,
    frames: animVimFrames,
    skip: 3100,
    speed: 1,
  },
  workflow: {
    image: animWorkflowSpritesheet,
    frames: animWorkflowFrames,
    skip: 500,
    speed: 1.25,
  },
};

function animate(canvas, feature) {
  let stop = false;
  let start = null;
  let frame = 0;

  const featureInfo = BIG_TABLE_OF_FEATURES[feature];
  const frames = featureInfo.frames;

  const image = new Image();
  image.src = featureInfo.image;

  {
    const [, , x1, y1, x2, y2] = frames[0][1][0];
    const w = x2 - x1;
    const h = y2 - y1;

    canvas.width = w;
    canvas.height = h;
  }

  const resizeCanvas = (w) => {
    const h = w * (canvas.height / canvas.width);
    canvas.style.height = `${h}px`;
  };

  resizeCanvas(canvas.getBoundingClientRect().width);

  const observer = new ResizeObserver((entries) => {
    entries.forEach((entry) => {
      if (entry.contentBoxSize) {
        resizeCanvas(entry.contentBoxSize[0].inlineSize);
      } else {
        resizeCanvas(entry.contentRect.width);
      }
    });
  });
  observer.observe(canvas);

  const ctx = canvas.getContext("2d");
  ctx.fillStyle = "rgb(26, 26, 26)";
  ctx.fillRect(0, 0, canvas.width * 2, canvas.height * 2);
  canvas.classList.remove("done");

  const draw = (rawTime) => {
    const ctx = canvas.getContext("2d");

    if (stop) return;
    if (!start) {
      start = rawTime;

      canvas.classList.add("done");
      ctx.fillStyle = "rgba(255, 255, 255, 0)";
      ctx.fillRect(0, 0, canvas.width * 2, canvas.height * 2);
    }

    let time = rawTime;
    time -= start;
    time += featureInfo.skip;
    time *= featureInfo.speed;

    while (frame < frames.length && time > frames[frame][0] - frames[0][0]) {
      frames[frame][1].forEach((change) => {
        const [x, y, x1, y1, x2, y2] = change;
        const w = x2 - x1;
        const h = y2 - y1;
        ctx.drawImage(image, x1, y1, w, h, x, y, w, h);
      });
      frame++;
    }

    if (frame === frames.length) {
      // start over
      start = null;
      frame = 0;
    }

    requestAnimationFrame(draw);
  };

  image.onload = () => requestAnimationFrame(draw);

  return () => {
    stop = true;
    observer.disconnect();
  };
}

async function preloadSpritesheets() {
  const sheets = [animSpritesheet, animVimSpritesheet, animWorkflowSpritesheet];
  for (let src of sheets) {
    await new Promise((resolve, reject) => {
      const img = new Image();
      img.src = src;
      img.onload = resolve;
      img.onerror = reject;
    });
  }
}

preloadSpritesheets();

function FeaturePresentation() {
  const [feature, setFeature] = React.useState("code");
  const [isCanvasReady, setIsCanvasReady] = React.useState(false);
  const canvasRef = React.useRef(null);
  const stopAnimRef = React.useRef(null);

  React.useEffect(() => {
    if (isCanvasReady) {
      stopAnimRef.current = animate(canvasRef.current, feature);
    }

    return () => {
      if (stopAnimRef.current) {
        stopAnimRef.current();
        stopAnimRef.current = null;
      }
    };
  }, [feature, isCanvasReady]);

  const canvasRefCallback = React.useCallback(
    (canvas) => {
      if (canvas) {
        canvasRef.current = canvas;
        setIsCanvasReady(true);
      }
    },
    [setIsCanvasReady]
  );

  return (
    <div
      className={cx(
        "flex flex-col",
        "lg:flex-row lg:px-8",
        "max-w-screen-xl mx-auto gap-8 px-3 items-center justify-center"
      )}
    >
      <div className="w-full relative">
        <canvas
          style={{ border: "solid 1px rgb(26, 26, 26)" }}
          className="w-full rounded-xl feature-canvas"
          ref={canvasRefCallback}
        />
        <div className="feature-loading absolute top-0 left-0 right-0 bottom-0 flex justify-center items-center opacity-30 transform scale-50">
          <Loading />
        </div>
      </div>
      <div
        className={cx(
          "w-full flex justify-between items-start gap-2",
          "sm:max-w-full sm:px-8",
          "md:gap-16 md:justify-start",
          "lg:w-60 lg:flex-col lg:gap-8 lg:px-0"
        )}
      >
        <Feature
          title="Code Intelligence"
          icon={codeSvg}
          selected={feature === "code"}
          onClick={() => setFeature("code")}
        >
          An IDE that understands what you're trying to do.
        </Feature>
        <Feature
          title="Vim Keybindings"
          icon={vimSvg}
          selected={feature === "vim"}
          onClick={() => setFeature("vim")}
        >
          Built into the core of the application. Designed to work seamlessly
          with everything else.
        </Feature>
        <Feature
          selected={feature === "workflow"}
          onClick={() => setFeature("workflow")}
          title={
            <>
              <span className="inline-block">Integrated</span>{" "}
              <span className="inline-block">Build &amp; Debug</span>
            </>
          }
          icon={workflowSvg}
        >
          Your entire workflow in one application.
        </Feature>
      </div>
    </div>
  );
}

function Home() {
  return (
    <div className="w-full">
      <div className="mt-8 px-4 sm:mt-24 sm:mb-8 md:mt-24 md:mb-12 lg:max-w-screen-xl lg:mx-auto">
        <div className="leading-tight text-center mb-2 text-4xl md:text-5xl font-bold text-black">
          <span className="inline-block">The Fastest</span>{" "}
          <span className="inline-block">IDE for Go</span>
        </div>
        <div className="mt-4 mb-6 md:mb-20 text-lg text-gray-400 text-center">
          <Link
            to="/beta"
            className="rounded-full bg-gray-100 hover:bg-gray-200 color-gray-400 inline-block text-sm py-1 px-4 no-underline"
          >
            <b className="text-black">NEW:</b> <Icon icon={FaApple} /> macOS now
            in private beta!
          </Link>
        </div>
      </div>

      <FeaturePresentation />

      <div
        className={cx(
          "mt-8 pt-8 mb-4 pb-12 px-6 bg-gray-100 ",
          "sm:pt-16 sm:pb-20",
          "md:mb-16 md:gap-16",
          "lg:flex lg:mt-24 lg:mb-4 lg:pt-16 lg:pb-20 lg:justify-center lg:items-center"
        )}
      >
        <div className="sm:max-w-md lg:max-w-2xl sm:mx-auto lg:flex lg:gap-12">
          <div className="flex justify-center mb-4">
            <img alt="gpu" src={gpuImage} className="w-48 lg:w-96" />
          </div>
          <div className="leading-relaxed">
            <h2 className="text-2xl mb-4 text-gray-800 lg:w-full font-medium">
              No Electron. No JavaScript. No garbage collection.
            </h2>
            <div className="lg:w-full md:mt-0 lg:mt-4 text-lg text-gray-600">
              <p>
                CodePerfect is written in C++ and runs at 144 FPS. Every
                keystroke responds instantly.
              </p>
              <p>
                A full-featured IDE, faster than Sublime. Tear through your code
                without being encumbered by your tools.
              </p>
              <p className="text-center lg:text-left">
                <Link className="button main-button mt-4" to="/beta">
                  Request Access
                </Link>
              </p>
            </div>
          </div>
        </div>
      </div>
      <Pricing />
    </div>
  );
}

function PricingBox({ title, monthly, yearly, isYearly, children, unit, cta }) {
  return (
    <div
      className={cx(
        "w-auto md:w-1/3 rounded-lg overflow-hidden p-8",
        "border border-gray-200 shadow-sm"
      )}
    >
      <h1 className="font-bold text-gray-700 text-lg mb-2">{title}</h1>
      <div className="flex items-end">
        <div className="font-medium text-black text-5xl">
          ${isYearly ? yearly : monthly}
        </div>
        <div className="leading-tight text-xs pb-1 ml-2">
          {unit && <div>per {unit}</div>}
          <div>per {isYearly ? "year" : "month"}</div>
        </div>
      </div>
      <div className="text-gray-500 text-left my-6">{children}</div>
      {cta}
    </div>
  );
}

function PricingPoint({ label, not }) {
  return (
    <div className={cx("leading-5 mb-1", not && "text-red-600")}>
      <Icon icon={not ? AiOutlineClose : AiOutlineCheck} /> {label}
    </div>
  );
}

function Card({ children, ...props }) {
  return (
    <div className="border border-gray-300 rounded mt-4" {...props}>
      {children}
    </div>
  );
}

function CardSection({ children, step, ...props }) {
  return (
    <div
      className="p-6 border-b border-gray-300 last:border-0 relative"
      {...props}
    >
      <div
        className={cx(
          "w-6 h-6 rounded-full text-gray-500 flex items-center justify-center",
          "bg-gray-200 text-sm absolute left-5 top-6"
        )}
      >
        <span>{step}</span>
      </div>
      <div className="ml-10">{children}</div>
    </div>
  );
}

function AutoInstall() {
  const code = getCode();
  const data = useAuthWeb(code);

  if (!data) {
    return <Loading />;
  }

  const installLink = `${API_BASE}/install?code=${code}`;

  return (
    <WallOfText>
      <Title>Install</Title>
      <Card>
        <CardSection step={1}>
          <div>Paste this into a terminal:</div>
          <div className="my-4">
            <Snippet text={`curl -s "${installLink}" | bash`} />
          </div>
          <div className="text-xs text-gray-400">
            CodePerfect requires Go 1.13+. If Go isn't intalled, the script will
            offer to install it with Homebrew. You can also decline and install
            Go yourself if you prefer.
          </div>
        </CardSection>
        <CardSection step={2}>
          That's it! See{" "}
          <A href="https://docs.codeperfect95.com/overview/getting-started/">
            Getting Started
          </A>{" "}
          to start using CodePerfect.
        </CardSection>
      </Card>
      <div className="flex justify-between mt-4">
        <A className="font-semibold no-underline" href={installLink}>
          <Icon icon={AiFillCode} /> View install script
        </A>

        <Link
          className="font-semibold no-underline"
          to={`/install/manual?code=${code}`}
        >
          Manual install <Icon icon={HiArrowNarrowRight} />
        </Link>
      </div>
    </WallOfText>
  );
}

function CopyButton({ text }) {
  const [check, setCheck] = React.useState(false);
  const timeout = React.useRef(null);

  const onCopy = React.useCallback(() => {
    setCheck(true);
    if (timeout.current) {
      clearTimeout(timeout.current);
    }
    timeout.current = setTimeout(() => setCheck(false), 750);
  }, []);

  return (
    <CopyToClipboard
      onCopy={onCopy}
      text={text}
      className="absolute top-2 right-2 cursor-pointer p-1.5 pb-1 shadow bg-white hover:text-black text-gray-600 leading-none rounded text-sm"
    >
      <span>
        <Icon icon={check ? FcCheckmark : FaRegClipboard} />
      </span>
    </CopyToClipboard>
  );
}

function Snippet({ text }) {
  return (
    <div className="relative group">
      <pre
        className="relative text-left border-0 overflow-auto"
        style={{
          "border-radius": "8px",
          background: "#eef2ee",
          color: "#383",
          padding: "0.75rem",
        }}
      >
        {text}
      </pre>
      <div className="absolute right-0 top-0 opacity-0 group-hover:opacity-100 transition-opacity">
        <CopyButton text={text} />
      </div>
    </div>
  );
}

function getCode() {
  return new URLSearchParams(window.location.search).get("code");
}

function useAuthWeb(code) {
  const [data, setData] = React.useState(null);
  const [error, setError] = React.useState(null);
  const history = useHistory();

  React.useEffect(() => {
    async function run() {
      const resp = await fetch(`${API_BASE}/auth-web`, {
        method: "POST",
        headers: {
          Accept: "application/json",
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ code: getCode() }),
      });

      const data = await resp.json();
      if (data.error) {
        setError(data.error);
        return;
      }

      setData(data);
    }
    run();
  }, [code, setData, setError]);

  React.useEffect(() => {
    if (error) {
      alert(error);
      history.push("/");
    }
  }, [error, history]);

  return data;
}

function Loading() {
  return (
    <div className="text-center my-24">
      <div className="lds-ring">
        <div></div>
        <div></div>
        <div></div>
        <div></div>
      </div>
    </div>
  );
}

function ManualInstall() {
  const code = getCode();
  const data = useAuthWeb(code);

  const onDownload = React.useCallback(
    async (os) => {
      const resp = await fetch(`${API_BASE}/download?code=${code}&os=${os}`);
      const text = await resp.text();

      if (resp.status !== 200) {
        let error;
        try {
          error = JSON.parse(text).error;
        } catch {
          error = "Unable to download.";
        }
        alert(error);
        return;
      }

      window.location.href = text;
    },
    [code]
  );

  if (!data) {
    return <Loading />;
  }

  return (
    <WallOfText>
      <Title>Manual Install</Title>
      <Card>
        <CardSection step={1}>
          Install Go (version 1.13+). If you don't have it, we recommend
          installing <A href="https://brew.sh/">Homebrew</A>, then running{" "}
          <code>brew install go</code>.
        </CardSection>
        <CardSection step={2}>
          Download the appropriate package for your machine:
          <p className="flex space-x-2">
            <button
              className="button download-button mr-1.5"
              onClick={() => onDownload("darwin")}
            >
              <Icon icon={FaApple} /> Mac &ndash; Intel
            </button>
            <button
              className="button download-button"
              onClick={() => onDownload("darwin_arm")}
            >
              <Icon icon={FaApple} /> Mac &ndash; M1
            </button>
          </p>
        </CardSection>
        <CardSection step={3}>
          Copy your license key into <code>~/.cplicense</code>:
          <div className="mt-4">
            <Snippet
              text={`{\n  "email": "${data.email}",\n  "key": "${data.license_key}"\n}`}
            />
          </div>
        </CardSection>
        <CardSection step={4}>
          CodePerfect needs to know where to find various things. Create a
          <code>~/.cpconfig</code> file:
          <div className="mt-4">
            <Snippet
              text={`{
  "go_binary_path": "...",
  "goroot": "...",
  "gomodcache": "..."
}`}
            />
          </div>
          <p>
            Fill these in with the values of <code>which go</code>,{" "}
            <code>go env GOROOT</code>, and <code>go env GOMODCACHE</code>.
          </p>
        </CardSection>
        <CardSection step={5}>
          That's it! See{" "}
          <A href="https://docs.codeperfect95.com/overview/getting-started/">
            Getting Started
          </A>{" "}
          to start using CodePerfect.
        </CardSection>
      </Card>
      <p>
        <Link to={`/install?code=${code}`} className="font-bold no-underline">
          <Icon icon={HiArrowNarrowLeft} /> Use install script
        </Link>
      </p>
    </WallOfText>
  );
}

function Download() {
  return <Redirect to={`/install?code=${getCode()}`} />;
}

function Textbox({ area, onChange, className, ...props }) {
  const realProps = {
    className: cx(className, "py-1.5 px-3 rounded border-2 border-gray-200"),
    onChange: onChange && ((e) => onChange(e.target.value)),
    ...props,
  };
  if (area) {
    realProps.className = cx(realProps.className, "resize-none h-24");
    return <textarea {...realProps} />;
  }
  return <input {...realProps} />;
}

function BetaField({ className, label, children }) {
  return (
    <div className={cx("my-6", className)}>
      {label && <div className="mb-1.5 font-medium">{label}</div>}
      <div>{children}</div>
    </div>
  );
}

/*
function BetaStep({ step, children }) {
  const selected = step === 1;
  return (
    <div
      className={cx(
        "p-4 rounded mb-3 flex items-center text-lg text-black",
        selected ? "border-r-4 border-blue-400" : "opacity-30"
      )}
    >
      <div className="w-10 h-10 mr-3 flex rounded-full items-center justify-center bg-gray-600 text-gray-200">
        <span>{step}</span>
      </div>
      <div>{children}</div>
    </div>
  );
}
*/

function Beta() {
  const [sending, setSending] = React.useState(false);
  const [form, rawSetForm] = React.useState({
    name: "",
    email: "",
    os: [],
    editor: [],
  });
  const [stage, setStage] = React.useState("signup");
  const setForm = (cb) => rawSetForm(produce(form, cb));

  const osOptions = [
    ["windows", "Windows"],
    ["mac", "macOS"],
    ["linux", "Linux"],
  ];

  const editorOptions = [
    ["goland", "Goland"],
    ["vscode", "VSCode"],
    ["text_editor", "A text editor (Vim, Sublime)"],
    ["other", "Other"],
  ];

  const onSubmit = React.useCallback(
    async (e) => {
      e.preventDefault();
      setSending(true);
      const resp = await fetch(`${API_BASE}/beta-signup`, {
        method: "POST",
        headers: {
          Accept: "application/json",
          "Content-Type": "application/json",
        },
        body: JSON.stringify(form),
      });
      const data = await resp.json();
      setStage(data.next_stage);
      setSending(false);
    },
    [form, setSending, setStage]
  );

  return (
    <WallOfText width="xl" className="border shadow">
      <Title>CodePerfect Beta</Title>
      {stage === "signup" && (
        <form onSubmit={onSubmit}>
          <BetaField label="What's your name?">
            <Textbox
              value={form.name}
              onChange={(name) =>
                setForm((draft) => {
                  draft.name = name;
                })
              }
              className="w-full"
            />
          </BetaField>
          <BetaField label="What's your email?">
            <Textbox
              value={form.email}
              onChange={(email) =>
                setForm((draft) => {
                  draft.email = email;
                })
              }
              className="w-full"
            />
          </BetaField>
          <BetaField label="What platforms do you use?">
            {osOptions.map(([key, label]) => (
              <div className="mb-1">
                <label className="mr-4 cursor-pointer">
                  <input
                    checked={form.os.includes(key)}
                    onChange={(e) => {
                      setForm((draft) => {
                        const set = new Set(draft.os);
                        if (e.target.checked) {
                          set.add(key);
                        } else {
                          set.delete(key);
                        }
                        draft.os = Array.from(set);
                      });
                    }}
                    type="checkbox"
                    className="mr-1 cursor-pointer"
                  />
                  {label}
                </label>
              </div>
            ))}
          </BetaField>
          <BetaField label="What do you currently use to write Go?">
            {editorOptions.map(([key, label]) => (
              <div className="mb-1">
                <label className="mr-4 cursor-pointer">
                  <input
                    checked={form.editor.includes(key)}
                    onChange={(e) => {
                      setForm((draft) => {
                        const set = new Set(draft.editor);
                        if (e.target.checked) {
                          set.add(key);
                        } else {
                          set.delete(key);
                        }
                        draft.editor = Array.from(set);
                      });
                    }}
                    type="checkbox"
                    className="mr-1 cursor-pointer"
                  />
                  {label}
                </label>
              </div>
            ))}
          </BetaField>
          <div>
            <button
              disabled={sending}
              type="submit"
              className="button main-button"
            >
              {sending ? "Submitting..." : "Join Beta"}
            </button>
          </div>
        </form>
      )}

      {stage === "not_supported" && (
        <p>
          Thanks for signing up! At the moment we only support macOS, but we'll
          be sure to update you when we roll out support for other platforms.
        </p>
      )}

      {stage === "supported" && (
        <p>
          Thanks for signing up! We just sent you an email to schedule your
          onboarding call. (If you don't see it, please check your spam folder.)
        </p>
      )}
    </WallOfText>
  );
}

function Anchor({ name }) {
  const ref = React.useCallback(
    (elem) => {
      if (elem) {
        if (window.location.hash.slice(1) === name) {
          setTimeout(() => elem.scrollIntoView(), 1);
        }
      }
    },
    [name]
  );

  // eslint-disable-next-line
  return <a ref={ref} name={name}></a>;
}

function Pricing() {
  const [yearly, setYearly] = React.useState(false);

  return (
    <div className="pricing my-24">
      <Anchor name="pricing" />
      <h1 className="text-center text-black font-bold text-4xl mb-8">
        Pricing
      </h1>
      <div className="text-center">
        <span className="relative inline-block text-sm">
          <button
            onClick={() => setYearly(false)}
            className={cx(
              "left-0 inline-block rounded-full py-1 px-3 font-medium cursor-pointer outline-none",
              !yearly ? "bg-gray-200 text-black" : "text-gray-400"
            )}
          >
            Monthly
          </button>
          <button
            onClick={() => setYearly(true)}
            className={cx(
              "left-0 inline-block rounded-full py-1 px-4 font-medium cursor-pointer outline-none",
              yearly ? "bg-gray-200 text-black" : "text-gray-400"
            )}
          >
            Annual
          </button>
        </span>
      </div>
      <div className="mt-8">
        <div className="px-4 md:px-4 max-w-5xl flex flex-col md:flex-row space-between space-y-4 space-x-0 md:space-y-0 md:space-x-4 lg:space-x-12 mx-auto">
          <PricingBox
            title="Personal"
            monthly={5}
            yearly={45}
            isYearly={yearly}
            cta={
              <Link className="button main-button" to="/beta">
                Request Access
              </Link>
            }
          >
            <PricingPoint label="7-day free trial" />
            <PricingPoint label="Commercial use allowed" />
            <PricingPoint label="All features unlocked" />
            <PricingPoint not label=" Company cannot pay" />
            <PricingPoint not label=" Purchase cannot be expensed" />
          </PricingBox>
          <PricingBox
            title="Team"
            monthly={10}
            yearly={90}
            unit={"user"}
            isYearly={yearly}
            cta={
              <Link className="button main-button" to="/beta">
                Request Access
              </Link>
            }
          >
            <PricingPoint label="All features in Personal" />
            <PricingPoint label="Company can pay" />
            <PricingPoint label="Purchase can be expensed" />
          </PricingBox>
          <PricingBox
            title="Premium"
            monthly="20+"
            yearly="180+"
            unit={"user"}
            isYearly={yearly}
            cta={
              <a
                className="button main-button"
                href="mailto:sales@codeperfect95.com"
              >
                Contact Sales
              </a>
            }
          >
            <PricingPoint label="All features in Team" />
            <PricingPoint label="Priority support" />
            <PricingPoint label="Custom requests &amp; integrations" />
          </PricingBox>
        </div>
      </div>
      {/*
      <div className="pt-16 mt-16 border-t border-gray-200 text-center">
        <p className="text-xl mb-0">Ready to get started?</p>
        <p>
          <A
            className="button main-button font-bold text-xl px-6 py-3 whitespace-nowrap"
            href={BETA_SIGNUP_LINK}
          >
            Request Access
          </a>
        </p>
      </div>
      */}
    </div>
  );
}

function Terms() {
  return (
    <>
      <WallOfText>
        <Title>Terms of Service</Title>
        <p>
          This website provides you with information about the IDE and a means
          for you to subscribe to our services, which allow you to use the IDE
          for as long as your subscription is active.
        </p>
        <p>
          The IDE is an application that lets you write Go applications. In
          exchange for paying a monthly rate, we provide you with a license to
          use it.
        </p>

        <br />

        <Title>Privacy Policy</Title>
        <p>
          When you fill out the Join Beta form, we collect your name and email.
          We use this to send you updates about new product features.
        </p>
        <p>
          When you sign up, we collect your name, email, and credit card
          information. We use this information to bill you and send you emails
          with updates about your payment status (for example, if your card
          fails).
        </p>
        <p>
          The IDE contacts the server to authenticate your license key and to
          install automatic updates. This exposes your IP address to us. We
          won't share it with anyone, unless ordered to by law.
        </p>
      </WallOfText>
    </>
  );
}

function ScrollToTop() {
  const { pathname, hash } = useLocation();

  React.useEffect(() => {
    if (!hash) {
      window.scrollTo(0, 0);
    }
  }, [pathname, hash]);

  return null;
}

function FinishSignup(props) {
  const params = useParams();
  const [data, setData] = React.useState(null);

  React.useEffect(() => {
    async function run() {
      const resp = await fetch(`${API_BASE}/airtable`, {
        method: "POST",
        headers: {
          Accept: "application/json",
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ airtable_id: params.id }),
      });
      const data = await resp.json();
      setData(data);
    }
    run();
  }, [params.id]);

  if (data === null) return <Loading />;

  return (
    <WallOfText width="2xl">
      <Title>Thanks for signing up for CodePerfect!</Title>
      {data.action === "schedule_call" && (
        <>
          <p>
            We're working closely with a small number of people for our private
            macOS beta release. The next step is to schedule an onboarding call.
          </p>
          <p>
            The goal is for us to learn about what projects you're working on
            and what your day-to-day looks like. After that we'll take you
            through CodePerfect installation and do a feature walkthrough.
          </p>
          <div className="mt-6">
            <A className="button main-button mr-2" href={data.call_link}>
              Schedule Call
              <Icon className="ml-2" icon={BsArrowRight} />
            </A>
            <a
              className="button main-button download-button"
              href={`mailto:${SUPPORT_EMAIL}`}
            >
              Contact Us
            </a>
          </div>
        </>
      )}
      {data.action === "nothing" && (
        <p>
          At the moment we're still rolling out support for your OS, but we'll
          reach out once we do.
        </p>
      )}
    </WallOfText>
  );
}

function App() {
  return (
    <Router>
      <ScrollToTop />

      <Helmet>
        <meta charSet="utf-8" />
        <title>CodePerfect 95</title>
      </Helmet>

      <div className="text-gray-500">
        <div className="pt-4 lg:pt-8 px-4 pb-4 flex justify-between items-center w-full lg:max-w-screen-xl lg:mx-auto">
          <Link
            to="/"
            className="font-bold text-lg text-black no-underline whitespace-nowrap flex items-center"
          >
            <img
              alt="logo"
              className="w-auto h-16 inline-block mr-4"
              src={logoImage}
            />
            <span className="hidden md:inline-block">CodePerfect 95</span>
          </Link>
          <div className="flex items-baseline space-x-6">
            <A
              className="no-underline font-semibold text-gray-600 hidden sm:inline-block"
              href="https://docs.codeperfect95.com"
            >
              Docs
            </A>

            <Link
              className="no-underline font-semibold text-gray-600 hidden sm:inline-block"
              to="/pricing"
            >
              Pricing
            </Link>

            <Link className="button main-button" role="button" to="/beta">
              Join Beta
            </Link>
          </div>
        </div>
        <div>
          <Switch>
            <Route path="/beta" exact>
              <Beta />
            </Route>
            <Route path="/download" exact>
              <Download />
            </Route>
            <Route path="/install" exact>
              <AutoInstall />
            </Route>
            <Route path="/install/manual" exact>
              <ManualInstall />
            </Route>
            <Route path="/pricing">
              <Redirect to="/#pricing" />
            </Route>
            <Route path="/terms">
              <Terms />
            </Route>
            <Route path="/privacy">
              <Redirect to="/terms" />
            </Route>
            <Route exact path="/s/:id">
              <FinishSignup />
            </Route>
            <Route exact path="/">
              <Home />
            </Route>
            <Route>
              <Redirect to="/" />
            </Route>
          </Switch>
        </div>
        <div
          className={cx(
            "px-4 pt-4 mb-8 lg:pt-8 lg:mb-12 flex flex-col justify-between",
            "lg:max-w-screen-xl lg:mx-auto sm:flex-row"
          )}
        >
          <div className="text-gray-500">
            &copy; {new Date().getFullYear()} CodePerfect 95
          </div>
          <div className="flex flex-col sm:flex-row space-x-0 sm:space-x-12 mt-2 sm:mt-0">
            <div className="sm:text-right sm:flex sm:flex-row sm:space-x-8">
              <div>
                <A
                  className="text-gray-500 no-underline"
                  href="https://codeperfect95.notion.site/codeperfect95/CodePerfect-95-Changelog-dcedf41014ef4de79690a5b5b54ebb33"
                >
                  Changelog
                </A>
              </div>
              <div>
                <Link className="text-gray-500 no-underline" to="/beta">
                  Join Beta
                </Link>
              </div>
              <div>
                <A
                  className="text-gray-500 no-underline"
                  href="https://docs.codeperfect95.com"
                >
                  Docs
                </A>
              </div>
              <div>
                <A
                  className="text-gray-500 no-underline"
                  href={`mailto:${SUPPORT_EMAIL}`}
                >
                  Support
                </A>
              </div>
              <div>
                <Link to="/terms" className="text-gray-500 no-underline">
                  Terms &amp; Privacy
                </Link>
              </div>
            </div>
          </div>
        </div>
      </div>
    </Router>
  );
}

export default App;
