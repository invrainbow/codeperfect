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
} from "react-router-dom";

import { AiOutlineCheck, AiOutlineClose, AiFillCode } from "react-icons/ai";
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

const NAME = "CodePerfect 95";
const NAME_SHORT = "CodePerfect";
const SUPPORT_EMAIL = "support@codeperfect95.com";
const CURRENT_YEAR = new Date().getFullYear();
const BETA_SIGNUP_LINK = "https://airtable.com/shraN38Z2jqQJVqbk";

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

function WallOfText({ children, className, ...props }) {
  return (
    <div
      className={cx(
        className,
        "wall-of-text p-4 my-4 md:my-12 md:p-8 leading-normal md:max-w-3xl md:mx-auto"
      )}
      {...props}
    >
      {children}
    </div>
  );
}

function Title({ children, ...props }) {
  return (
    <h2 className="text-xl font-bold text-gray-700" {...props}>
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
    skip: 2100,
    speed: 1,
  },
  workflow: {
    image: animWorkflowSpritesheet,
    frames: animWorkflowFrames,
    skip: 500,
    speed: 1.25,
  },
};

class Anim {
  constructor(canvas, feature) {
    this.canvas = canvas;
    this.stop = false;
    this.start = null;
    this.frame = 0;

    this.feature = feature;

    const featureInfo = BIG_TABLE_OF_FEATURES[feature];
    this.frames = featureInfo.frames;
    this.skip = featureInfo.skip;
    this.speed = featureInfo.speed;

    this.image = new Image();
    this.image.src = featureInfo.image;
    this.image.onload = () => {
      if (!this.stop) {
        requestAnimationFrame(this.draw);
      }
    };

    this.firstFrameTime = this.frames[0][0];

    {
      const [, , x1, y1, x2, y2] = this.frames[0][1][0];
      const w = x2 - x1;
      const h = y2 - y1;

      this.canvas.width = w;
      this.canvas.height = h;
    }

    const resizeCanvas = (width) => {
      const height = width * (this.canvas.height / this.canvas.width);
      this.canvas.style.height = `${height}px`;
    };

    resizeCanvas(this.canvas.getBoundingClientRect().width);

    this.observer = new ResizeObserver((entries) => {
      for (let entry of entries) {
        if (entry.contentBoxSize) {
          resizeCanvas(entry.contentBoxSize[0].inlineSize);
        } else {
          resizeCanvas(entry.contentRect.width);
        }
      }
    });
    this.observer.observe(this.canvas);

    // clear canvas
    const ctx = this.canvas.getContext("2d");
    ctx.fillStyle = "rgb(26, 26, 26)";
    ctx.fillRect(0, 0, this.canvas.width * 2, this.canvas.height * 2);
  }

  cleanup() {
    this.stop = true;
    this.observer.disconnect();
  }

  draw = (time) => {
    if (!this.start) {
      this.start = time;
      // this.clearCanvas();
    }

    time -= this.start;
    time += this.skip;

    while (
      this.frame < this.frames.length &&
      time * this.speed > this.frames[this.frame][0] - this.firstFrameTime
    ) {
      this.frames[this.frame][1].forEach((change) => {
        const [x, y, x1, y1, x2, y2] = change;
        const ctx = this.canvas.getContext("2d");
        const [sx, sy] = [x1 + 2, y1 + 2];
        const [w, h] = [x2 - x1 - 4, y2 - y1 - 4];
        const [dx, dy] = [x + 2, y + 2];
        ctx.drawImage(this.image, sx, sy, w, h, dx, dy, w, h);
      });
      this.frame++;
    }

    if (this.frame === this.frames.length) {
      // start over
      this.start = null;
      this.frame = 0;
    }

    if (!this.stop) requestAnimationFrame(this.draw);
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
  const canvasRef = React.useRef(null);
  const animRef = React.useRef(null);

  function cleanupAnim() {
    if (animRef.current !== null) {
      animRef.current.cleanup();
      animRef.current = null;
    }
  }

  const initAnim = React.useCallback(() => {
    cleanupAnim();
    animRef.current = new Anim(canvasRef.current, feature);
  }, [feature]);

  React.useEffect(() => {
    initAnim();
  }, [initAnim, feature]);

  const canvasRefCallback = React.useCallback(
    (canvas) => {
      if (canvas) {
        canvasRef.current = canvas;
        if (animRef.current === null) {
          initAnim();
        }
      }
    },
    [initAnim]
  );

  // cleanup
  React.useEffect(() => {
    return () => cleanupAnim();
  }, []);

  return (
    <div
      className={cx(
        "flex flex-col",
        "lg:flex-row lg:px-8",
        "max-w-screen-xl mx-auto gap-8 px-3 items-center justify-center"
      )}
    >
      <div className="w-full">
        <canvas
          className="w-full border border-gray-300 shadow rounded-lg"
          ref={canvasRefCallback}
        />
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
          <A
            href={BETA_SIGNUP_LINK}
            className="rounded-full bg-gray-100 hover:bg-gray-200 color-gray-400 inline-block text-sm py-1 px-4 no-underline"
          >
            <b className="text-black">NEW:</b> <Icon icon={FaApple} /> macOS now
            in private beta!
          </A>
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
                {NAME_SHORT} is written in C++ and runs at 144 FPS. Every
                keystroke responds instantly.
              </p>
              <p>
                A full-featured IDE, faster than Sublime. Tear through your code
                without being encumbered by your tools.
              </p>
              <p className="text-center lg:text-left">
                <A className="main-button mt-4" href={BETA_SIGNUP_LINK}>
                  Request Access
                </A>
              </p>
            </div>
          </div>
        </div>
      </div>
      <Pricing />
    </div>
  );
}

function PricingBox({
  title,
  monthly,
  yearly,
  isYearly,
  children,
  unit,
  cta,
  link,
}) {
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
      <A className="main-button" href={link}>
        {cta}
      </A>
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
      <div className="p-6 border border-gray-300 rounded mt-4">
        <div>
          Make sure Go (version 1.13 or higher) is installed. Then paste this
          into a terminal:
        </div>
        <div className="mt-4">
          <Snippet text={`curl -s ${installLink} | bash`} />
        </div>
        <p>
          That's it! See{" "}
          <A href="https://docs.codeperfect95.com/overview/getting-started/">
            Getting Started
          </A>{" "}
          to start using CodePerfect.
        </p>
      </div>
      <div className="flex justify-between mt-3">
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
    <pre className="text-left rounded-md p-3 bg-gray-100 my-4 border-0 relative overflow-auto">
      <CopyButton text={text} />
      {text}
    </pre>
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
      <p>Install Go (version 1.13+ or higher).</p>
      <p>Download the appropriate package for your machine:</p>
      <p className="flex space-x-2">
        <button
          className="main-button download-button"
          onClick={() => onDownload("darwin")}
        >
          <Icon icon={FaApple} /> Mac &ndash; Intel
        </button>
        <button
          className="main-button download-button"
          onClick={() => onDownload("darwin_arm")}
        >
          <Icon icon={FaApple} /> Mac &ndash; M1
        </button>
      </p>
      <p>Unzip CodePerfect.app into your Applications folder.</p>

      <p>
        Here's your license key; copy it into <code>~/.cplicense</code>:
      </p>
      <div className="mt-4">
        <Snippet
          text={`{\n  "email": "${data.email}",\n  "key": "${data.license_key}"\n}`}
        />
      </div>
      <p>
        CodePerfect needs to know where to find various things. Create a
        <code>~/.cpconfig</code> file:
      </p>
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

function Anchor({ name }) {
  const ref = React.useCallback(
    (elem) => {
      console.log("ref found", elem);
      console.log(name);
      console.log(window.location.hash);
      if (window.location.hash.slice(1) === name) {
        console.log("scrolling");
        setTimeout(() => {
          elem.scrollIntoView();
        }, 1);
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
            cta="Request Access"
            link={BETA_SIGNUP_LINK}
          >
            <div className="">
              <Icon icon={AiOutlineCheck} /> Commercial use allowed
            </div>
            <div className="">
              <Icon icon={AiOutlineCheck} /> All features unlocked
            </div>
            <div className="text-red-600">
              <Icon icon={AiOutlineClose} /> Company cannot pay
            </div>
            <div className="text-red-600">
              <Icon icon={AiOutlineClose} /> Purchase cannot be expensed
            </div>
          </PricingBox>
          <PricingBox
            title="Team"
            monthly={10}
            yearly={90}
            unit={"user"}
            isYearly={yearly}
            cta="Request Access"
            link={BETA_SIGNUP_LINK}
          >
            <div className="">
              <Icon icon={AiOutlineCheck} /> All features in Personal
            </div>
            <div className="">
              <Icon icon={AiOutlineCheck} /> Company can pay
            </div>
            <div className="">
              <Icon icon={AiOutlineCheck} /> Purchase can be expensed
            </div>
          </PricingBox>
          <PricingBox
            title="Premium"
            monthly="20+"
            yearly="180+"
            unit={"user"}
            isYearly={yearly}
            cta="Contact Sales"
            link="mailto:sales@codeperfect95.com"
          >
            <div className="">
              <Icon icon={AiOutlineCheck} /> All features in Team
            </div>
            <div className="">
              <Icon icon={AiOutlineCheck} /> Priority support
            </div>
            <div className="">
              <Icon icon={AiOutlineCheck} /> Custom requests &amp; integrations
            </div>
          </PricingBox>
        </div>
      </div>
      {/*
      <div className="pt-16 mt-16 border-t border-gray-200 text-center">
        <p className="text-xl mb-0">Ready to get started?</p>
        <p>
          <A
            className="main-button font-bold text-xl px-6 py-3 whitespace-nowrap"
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

function App() {
  return (
    <Router>
      <ScrollToTop />

      <Helmet>
        <meta charSet="utf-8" />
        <title>{NAME}</title>
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
            <span className="hidden md:inline-block">{NAME}</span>
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

            <A className="main-button" role="button" href={BETA_SIGNUP_LINK}>
              Join Beta
            </A>
          </div>
        </div>
        <div className="">
          <Switch>
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
            &copy; {CURRENT_YEAR} {NAME}
          </div>
          <div className="flex flex-col sm:flex-row space-x-0 sm:space-x-12 mt-2 sm:mt-0">
            <div className="sm:text-right sm:flex sm:flex-row sm:space-x-6">
              <div>
                <A
                  className="text-gray-500 no-underline"
                  href={BETA_SIGNUP_LINK}
                >
                  Join Beta
                </A>
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
