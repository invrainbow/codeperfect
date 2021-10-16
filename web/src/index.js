import React from "react";
import ReactDOM from "react-dom";
import cx from "classnames";
import _ from "lodash";
import { Helmet } from "react-helmet";
import { AiOutlineCheck, AiOutlineClose, AiFillCode } from "react-icons/ai";
import { FaApple, FaRegClipboard } from "react-icons/fa";
import { FcCheckmark } from "react-icons/fc";
import { HiArrowNarrowRight } from "react-icons/hi";
import {
  BrowserRouter as Router,
  Link,
  Route,
  Switch,
  useLocation,
  Redirect,
  useHistory,
} from "react-router-dom";
import { CopyToClipboard } from "react-copy-to-clipboard";
import "./index.css";
import gif60fps from "./60fps.gif";
import ideScreenshotImage from "./ide.png";
import pngIntellisense from "./intellisense.png";
import logoImage from "./logo.png";
import gifVim from "./vim.gif";

// constants
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

function WallOfText({ title, children }) {
  return (
    <div className="wall-of-text p-4 my-12 md:p-8 leading-normal md:max-w-3xl md:mx-auto">
      {title && <Title>{title}</Title>}
      {children}
    </div>
  );
}

function Icon({ icon }) {
  const C = icon;
  return (
    <span className="relative -top-0.5">
      <C />
    </span>
  );
}

function Title({ children, ...props }) {
  return (
    <h2 className="text-xl font-bold text-gray-700" {...props}>
      {children}
    </h2>
  );
}

function NiceImage({ className, ...props }) {
  return (
    <div className={cx("flex items-center justify-center", className)}>
      <img alt="" className="overflow-hidden w-full" {...props} />
    </div>
  );
}

function Section({ className, ...props }) {
  return (
    <div
      className={cx(
        "mb-24 md:gap-14 flex flex-col md:flex-row md:mb-32 md:items-center",
        className
      )}
      {...props}
    />
  );
}

const IDE_FEATURES = _.shuffle([
  "Auto Completion",
  "Integrated Debugger",
  "Integrated Build",
  "Auto Format",
  "Parameter Hints",
  "Go To Definition",
  "Vim Keybindings",
  "Fuzzy File Search",
  "GPU-Based Renderer",
  "Highly Optimized Core",
  "Instant Startup",
  "Syntax Highlighting",
]);

function Home() {
  return (
    <div className="w-full lg:max-w-screen-xl lg:mx-auto">
      <div className="mt-8 mb-24 sm:my-24 md:my-32 md:mt-24">
        <div className="leading-tight text-center mb-2 text-4xl md:text-5xl font-bold text-black">
          The Fastest IDE for Go
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
        <img
          alt=""
          className="max-w-auto border border-gray-500 rounded-2xl"
          src={ideScreenshotImage}
        />
      </div>

      <Section className="md:block lg:flex">
        <div className="leading-relaxed mb-4 md:flex md:flex-row lg:block md:gap-8 md:mb-4 lg:w-1/3">
          <h2 className="text-2xl mb-4 text-gray-700 md:w-4/12 lg:w-full font-medium">
            No Electron. No JavaScript. No garbage collection.
          </h2>
          <p className="md:w-4/12 lg:w-full md:mt-0 lg:mt-4">
            {NAME_SHORT} is written in C++ and designed to run at 144 FPS. Every
            keystroke responds instantly.
          </p>
        </div>
        <NiceImage src={gif60fps} className="lg:w-2/3" />
      </Section>

      <Section className="md:flex-row-reverse">
        <div className="leading-relaxed md:w-5/12">
          <h2 className="text-2xl mb-4 text-gray-700 font-medium">
            A batteries-included IDE, as fast as Vim.
          </h2>
          <p>
            Today, IDEs are powerful but slow, while Vim is fast but limited.{" "}
            {NAME_SHORT} brings the best of both worlds: everything you need to
            effectively develop in Go, at lightning speed.
          </p>
          <p>
            No more waiting on GoLand. No more hacking Vim plugins together.
          </p>
        </div>
        <div className="md:w-7/12 mt-8 md:mt-0 grid grid-cols-3 sm:grid-cols-4 md:grid-cols-4 lg:grid-cols-4 gap-3 md:gap-6 select-none">
          {IDE_FEATURES.map((feature, i) => (
            <div
              key={i}
              className="text-sm md:text-md leading-tight font-semibold h-16 md:h-20 lg:h-20 rounded-lg text-gray-500 hover:text-gray-600 text-center flex justify-center items-center p-4 transform hover:scale-110 transition-all shadow-sm"
              style={{ background: "#eee" }}
            >
              <span>{feature}</span>
            </div>
          ))}
        </div>
      </Section>

      <div className="block md:hidden">
        <Section>
          <div className="leading-relaxed md:flex md:flex-row md:gap-8 md:mb-12">
            <div className="text-2xl mb-4 text-gray-700 md:w-1/3 font-medium">
              Tools that understand Go as a first language.
            </div>
            <p className="md:w-1/3 md:mt-0">
              {NAME_SHORT}'s intellisense is hand-written, extensively tested,
              and designed for speed and reliability.
            </p>
          </div>
        </Section>
      </div>

      <div className="hidden md:block relative rounded-md overflow-hidden border-gray-500 border mb-24 md:mb-32">
        <div
          className="leading-relaxed md:gap-8 w-5/12 z-20 relative p-8 drop-shadow rounded-md my-12 mx-auto"
          style={{ background: "rgba(0, 0, 0, 0.75)" }}
        >
          <div className="text-2xl mb-4 text-white font-medium">
            Tools that understand Go as a first language.
          </div>
          <p className="text-white">
            {NAME_SHORT}'s intellisense is hand-written, extensively tested, and
            designed for speed and reliability.
          </p>
        </div>
        <img
          src={pngIntellisense}
          alt=""
          className="absolute top-0 left-0 z-10 w-full opacity-50"
        />
      </div>

      <Section className="md:flex-row-reverse">
        <div className="leading-relaxed mb-4 md:mb-0 md:w-5/12">
          <div className="text-2xl mb-4 text-gray-700 font-medium">
            Complete Vim keybindings, out of the box.
          </div>
          <p>
            Our Vim keybindings aren't a plugin added as an afterthought.
            They're built into the core of the application and designed to work
            seamlessly with everything else.
          </p>
        </div>
        <NiceImage src={gifVim} className="md:w-7/12" />
      </Section>
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
    <WallOfText title="Install">
      <div className="p-6 border border-gray-300 rounded mt-4">
        <div>Install Go (version 1.13 or higher), then run:</div>
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
    <WallOfText title="Manual Install">
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
    </WallOfText>
  );
}

function Download() {
  return <Redirect to={`/install?code=${getCode()}`} />;
}

function Pricing() {
  const [yearly, setYearly] = React.useState(false);

  return (
    <div className="pricing my-24">
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
        <div className="max-w-5xl flex flex-col md:flex-row space-between space-y-8 space-x-0 md:space-y-0 md:space-x-12 mx-auto">
          <PricingBox
            title="Personal"
            monthly={5}
            yearly={45}
            isYearly={yearly}
            cta="Request access"
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
            cta="Request access"
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
            cta="Contact sales"
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
    <WallOfText title="Terms of Service">
      <p>
        This website provides you with information about the IDE and a means for
        you to subscribe to our services, which allow you to use the IDE for as
        long as your subscription is active.
      </p>
      <p>
        The IDE is an application that lets you write Go applications. In
        exchange for paying a monthly rate, we provide you with a license to use
        it.
      </p>
    </WallOfText>
  );
}

function Privacy() {
  return (
    <WallOfText title="Privacy Policy">
      <p>
        When you fill out the Join Beta form, we collect your name and email. We
        use this to send you updates about new product features.
      </p>
      <p>
        When you sign up, we collect your name, email, and credit card
        information. We use this information to bill you and send you emails
        with updates about your payment status (for example, if your card
        fails).
      </p>
      <p>
        The IDE contacts the server to authenticate your license key and to
        install automatic updates. This exposes your IP address to us. We won't
        share it with anyone, unless ordered to by law.
      </p>
    </WallOfText>
  );
}

function ScrollToTop() {
  const { pathname } = useLocation();

  React.useEffect(() => {
    window.scrollTo(0, 0);
  }, [pathname]);

  return null;
}

ReactDOM.render(
  <React.StrictMode>
    <Router>
      <ScrollToTop />

      <Helmet>
        <meta charSet="utf-8" />
        <title>{NAME}</title>
      </Helmet>

      <div className="p-6 md:p-12 text-gray-500">
        <div className="pb-4 flex justify-between items-center w-full lg:max-w-screen-xl lg:mx-auto">
          <Link
            to="/"
            className="font-bold text-lg text-black no-underline whitespace-nowrap flex items-center"
          >
            <img
              alt="logo"
              className="inline-block w-auto h-16 inline-block mr-4"
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
              <Pricing />
            </Route>
            <Route path="/terms">
              <Terms />
            </Route>
            <Route path="/privacy">
              <Privacy />
            </Route>
            <Route exact path="/">
              <Home />
            </Route>
            <Route>
              <Redirect to="/" />
            </Route>
          </Switch>
        </div>
        <div className="lg:max-w-screen-xl lg:mx-auto flex flex-col sm:flex-row justify-between">
          <div className="text-gray-500">
            &copy; {CURRENT_YEAR} {NAME}
          </div>
          <div className="flex flex-col sm:flex-row space-x-0 sm:space-x-12 mt-2 sm:mt-0">
            <div className="text-left">
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
                <Link to="/pricing" className="text-gray-500 no-underline">
                  Pricing
                </Link>
              </div>
            </div>
            <div className="text-left">
              <div>
                <Link to="/terms" className="text-gray-500 no-underline">
                  Terms of Service
                </Link>
              </div>
              <div>
                <Link className="text-gray-500 no-underline" to="/privacy">
                  Privacy Policy
                </Link>
              </div>
              <div>
                <A
                  className="text-gray-500 no-underline"
                  href={`mailto:${SUPPORT_EMAIL}`}
                >
                  Contact
                </A>
              </div>
            </div>
          </div>
        </div>
      </div>
    </Router>
  </React.StrictMode>,
  document.getElementById("root")
);
