// /* global Stripe */

import React from "react";
import ReactMarkdown from "react-markdown";
import {
  BrowserRouter as Router,
  Switch,
  Route,
  Link,
  useLocation,
} from "react-router-dom";
import { Helmet } from "react-helmet";
import cx from "classnames";
import _ from "lodash";
import { PlayIcon } from "@heroicons/react/solid";
import rehypeRaw from "rehype-raw";

// static assets
import ideScreenshotImage from "./ide.png";
import gif60fps from "./60fps.gif";
import gifVim from "./vim.gif";
import pngIntellisense from "./intellisense.png";
import pngVideoScreenshot from "./video-screenshot.png";
import philosophyEssay from "./philosophy.txt";

// constants
const IDE_NAME = "CodePerfect 95";
const SUPPORT_EMAIL = "support@codeperfect95.com";
const API_BASE = "http://localhost:8080";
// const API_BASE = "https://api.codeperfect95.com";
const CURRENT_YEAR = new Date().getFullYear();

// const STRIPE_PRICE_ID = "price_1IrHFLBpL0Zd3zdOjGoBlmZF";
// const STRIPE_PUB_KEY =
//   "pk_test_51IqLcpBpL0Zd3zdOPzAZQbYjpmD47ethoqtcFGwiJBLdovijF8G0hBTA8FylfnRnQ8aXoPVC2DmNMHpndiV1YtJr00UU0XCWnt";

// const stripe = Stripe(STRIPE_PUB_KEY);

function WallOfText({ title, children }) {
  return (
    <div className="wall-of-text border-t border-b md:border border-gray-700 py-5 md:p-12 md:rounded-sm leading-normal">
      <div className="md:max-w-2xl md:mx-auto">
        <Title>{title}</Title>
        {children}
      </div>
    </div>
  );
}

function Title({ children, ...props }) {
  return (
    <h2 className="text-lg font-semibold text-gray-200" {...props}>
      {children}
    </h2>
  );
}

function NiceImage({ className, ...props }) {
  return (
    <div className={cx("flex items-center justify-center", className)}>
      <img alt="" className="overflow-hidden nice-image w-full" {...props} />
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
  // "Debug Tests",
  "Instant Startup",
  "Syntax Highlighting",
  // "Navigate Errors",
]);

function Home() {
  return (
    <div>
      <div className="mt-8 mb-24 sm:my-24 md:my-32">
        <div className="leading-tight text-center mb-6 md:mb-12 text-4xl md:text-5xl font-medium text-white">
          A Blazing Fast IDE for Go
        </div>
        <img
          alt=""
          className="nice-image max-w-auto"
          src={ideScreenshotImage}
        />
      </div>

      <Section className="md:block lg:flex">
        <div className="leading-relaxed mb-4 md:flex md:flex-row lg:block md:gap-8 md:mb-4 lg:w-1/3">
          <div className="text-2xl mb-4 text-gray-200 md:w-4/12 lg:w-full">
            No Electron. No JavaScript. No garbage collection.
          </div>
          <p className="md:w-4/12 lg:w-full md:mt-0 lg:mt-4">
            {IDE_NAME} is written in C++ and designed to run at 144 FPS. Every
            keystroke responds instantly.
          </p>
          <p className="md:w-4/12 lg:w-full md:mt-0 lg:mt-4">
            Modern laptop CPUs perform billions of ops/second. None of your apps
            should ever lag. {IDE_NAME} never does.
          </p>
        </div>
        <NiceImage src={gif60fps} className="lg:w-2/3" />
      </Section>

      <Section className="md:flex-row-reverse">
        <div className="leading-relaxed md:w-5/12">
          <div className="text-2xl mb-4 text-gray-200">
            A batteries-included IDE, as fast as Vim.
          </div>
          <p>
            Today, IDEs are powerful but slow, while Vim is fast but limited.
          </p>
          <p>
            {IDE_NAME} brings the best of both worlds: everything you need to
            effectively develop in Go, at lightning speed.
          </p>
          <p>
            No more waiting on GoLand's garbage collector. No more hacking Vim
            plugins together.
          </p>
        </div>
        <div className="md:w-7/12 mt-8 md:mt-0 grid grid-cols-3 sm:grid-cols-4 md:grid-cols-4 lg:grid-cols-4 gap-3 md:gap-6 select-none">
          {IDE_FEATURES.map((feature, i) => (
            <div
              key={i}
              className="font-semibold h-16 md:h-20 lg:h-20 rounded-lg text-gray-400 hover:text-gray-300 text-center flex justify-center items-center text-xs lg:text-sm p-4 transform hover:scale-110 transition-all shadow-sm"
              style={{ background: "#1d1d1d", border: "solid 1px #555" }}
            >
              <span>{feature}</span>
            </div>
          ))}
        </div>
      </Section>

      <div className="block md:hidden">
        <Section>
          <div className="leading-relaxed md:flex md:flex-row md:gap-8 md:mb-12">
            <div className="text-2xl mb-4 text-gray-200 md:w-1/3">
              A toolkit that understands Go as a first language.
            </div>
            <p className="md:w-1/3 md:mt-0">
              {IDE_NAME}'s intellisense is hand-written, extensively tested, and
              designed for speed and reliability.
            </p>
            <p className="md:w-1/3 md:mt-0">
              No more sending a JSON packet over a socket every keystroke. No
              more restarting your IDE because gopls crashed.
            </p>
          </div>
          {/* <NiceImage src={pngIntellisense} className="md:h-auto" /> */}
        </Section>
      </div>

      <div className="hidden md:block relative rounded-md overflow-hidden border-gray-500 border mb-24 md:mb-32">
        <div
          className="leading-relaxed md:gap-8 absolute top-8 left-0 w-5/12 z-20 p-8 drop-shadow rounded-tr-md rounded-br-md"
          style={{ background: "rgba(0, 0, 0, 0.5)" }}
        >
          <div className="text-2xl mb-4 text-gray-200">
            A toolkit that understands Go as a first language.
          </div>
          <p className="">
            {IDE_NAME}'s intellisense is hand-written, extensively tested, and
            designed for speed and reliability.
          </p>
          <p className="">
            No more sending a JSON packet over a socket every keystroke. No more
            restarting your IDE because gopls crashed.
          </p>
        </div>
        <img
          src={pngIntellisense}
          alt=""
          className="relative z-10 w-full opacity-40"
        />
      </div>

      <Section className="md:flex-row-reverse">
        <div className="leading-relaxed mb-4 md:mb-0 md:w-5/12">
          <div className="text-2xl mb-4 text-gray-200">
            Complete Vim keybindings, out of the box.
          </div>
          <p>
            Our Vim keybindings aren't a plugin added as an afterthought.
            They're built into the core of the application and designed to work
            seamlessly with everything else.
          </p>
          <p>
            We're big Vim users ourselves, and designed {IDE_NAME} with
            first-class Vim support from the beginning. It basically feels like
            real Vim.
          </p>
        </div>
        <NiceImage src={gifVim} className="md:w-7/12" />
      </Section>

      <p className="text-center mb-6">
        Watch a demo of {IDE_NAME} being used to solve{" "}
        <a target="_blank" rel="noreferrer" href="https://cryptopals.com">
          Cryptopals
        </a>
        .
      </p>

      <div className="mx-auto mb-16 md:mb-32 md:w-3/4">
        <a
          className="relative border shadow-md group"
          style={{ borderColor: "#888" }}
          rel="noreferrer"
          target="_blank"
          href="https://www.youtube.com/watch?v=9Q0YFHHLG-g"
        >
          <div className="absolute z-10 flex justify-center items-center w-full h-full">
            <PlayIcon className="group-hover:opacity-70 h-16 w-16 text-white drop-shadow opacity-50" />
          </div>
          <img
            alt=""
            src={pngVideoScreenshot}
            className="relative z-0 block opacity-30"
          />
        </a>
      </div>
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
        When you sign up, we collect your name, email, and credit card
        information. We use this information to bill you and send you emails
        with updates about your payment status (for example, if your card
        fails).
      </p>
      <p>
        When you sign up for our newsletter we collect your name and email. We
        use this to send you updates about new product features.
      </p>
      <p>
        The IDE contacts the server to authenticate your license key and to
        install automatic updates. This exposes your IP address to us. We won't
        share it with anyone, unless required to by law.
      </p>
    </WallOfText>
  );
}

// I'm just going to use POST and form data for everything. Do not talk to me
// about REST or any stupid shit like that or I'll fire you.
async function talkToServer(endpoint, params) {
  const makeFormData = (obj) => {
    const ret = new URLSearchParams();
    Object.keys(obj).forEach((key) => {
      ret.append(key, obj[key]);
    });
    return ret;
  };
  const resp = await fetch(`${API_BASE}/${endpoint}`, {
    method: "POST",
    body: makeFormData(params),
  });
  return await resp.json();
}

function Portal() {
  const [disabled, setDisabled] = React.useState(false);
  const [licenseKey, setLicenseKey] = React.useState(
    "ba26c910-e37e71e3-5e07eaf4-4b844592"
  );

  const onSubmit = async (e) => {
    e.preventDefault();
    setDisabled(true);
    try {
      const data = await talkToServer("portal", {
        license_key: licenseKey,
      });

      if (!data.portal_url) {
        alert("Invalid license key.");
        return;
      }

      window.location.href = data.portal_url;
    } finally {
      setDisabled(false);
    }
  };

  return (
    <WallOfText title="Portal Login">
      <p>
        Please enter your license key to sign in to the portal. It should be in
        the email that sent you here.
      </p>
      <form onSubmit={onSubmit}>
        <input
          type="text"
          value={licenseKey}
          onChange={(e) => setLicenseKey(e.target.value)}
          placeholder="License key"
          className="border border-black-400 py-1.5 px-2.5 rounded-md text-sm mr-2 font-mono"
        />
        <button
          className="main-button from-gray-200 to-gray-300 text-gray-600"
          disabled={disabled}
          type="submit"
        >
          Log in to portal
        </button>
      </form>
    </WallOfText>
  );
}

// const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

function Download() {
  const [disabled, setDisabled] = React.useState(false);
  const [licenseKey, setLicenseKey] = React.useState(
    "ba26c910-e37e71e3-5e07eaf4-4b844592"
  );

  const onSubmit = async (e) => {
    e.preventDefault();
    setDisabled(true);
    try {
      const data = await talkToServer("download", {
        license_key: licenseKey,
      });

      if (!data.download_link) {
        alert("Invalid license key.");
        return;
      }

      window.location.href = data.download_link;
    } finally {
      setDisabled(false);
    }
  };

  return (
    <WallOfText title="Download">
      <p>
        Please enter your license key to continue to the download. It should be
        in the email you got.
      </p>
      <form onSubmit={onSubmit}>
        <input
          type="text"
          value={licenseKey}
          onChange={(e) => setLicenseKey(e.target.value)}
          placeholder="License key"
          className="border border-black-400 py-1.5 px-2.5 rounded-md text-sm mr-2 font-mono"
        />
        <button
          className="main-button from-gray-200 to-gray-300 text-gray-600"
          type="submit"
          disabled={disabled}
        >
          Download
        </button>
      </form>
    </WallOfText>
  );
}

function Philosophy() {
  const [text, setText] = React.useState(null);

  React.useEffect(() => {
    async function run() {
      console.log(philosophyEssay);
      const resp = await fetch(philosophyEssay);
      const text = await resp.text();
      const newText = text.replaceAll("${IDE_NAME}", IDE_NAME);
      setText(newText);
    }
    run();
  }, []);

  return (
    <WallOfText title={`The ${IDE_NAME} Philosophy`}>
      {text ? (
        <ReactMarkdown rehypePlugins={[rehypeRaw]} children={text} />
      ) : (
        "Loading..."
      )}
    </WallOfText>
  );
}

function Beta() {
  // const [disabled, setDisabled] = React.useState(false);

  // const onBuy = async () => {
  //   setDisabled(true);
  //   try {
  //     const data = await talkToServer("checkout", {
  //       price_id: STRIPE_PRICE_ID,
  //     });
  //     stripe.redirectToCheckout({ sessionId: data.session_id });
  //   } catch (err) {
  //     setDisabled(false);
  //     alert(err);
  //   }
  // };

  return (
    <div
      className="border-t border-b py-4 md:py-12"
      style={{ borderColor: "#282828" }}
    >
      <div
        className="relative md:grid md:space-x-12"
        style={{ "grid-template-columns": "33% 67%" }}
      >
        <div
          className="sticky top-4 mb-4 md:top-4 md:self-start border border-gray-700 rounded-md shadow-md overflow-hidden"
          style={{ background: "#151515" }}
        >
          <div className="text-sm text-center md:text-lg md:border-b border-gray-700 pb-2 md:pt-4 p-4 md:p-4">
            <b className="text-white">$10/month</b> per seat
          </div>

          <div
            className="text-center md:border-b md:border-gray-700 pt-0 md:pt-4 p-4"
            style={{ background: "#111" }}
          >
            {/* <button
                onClick={onBuy}
                className="main-button text-lg px-8 py-4"
                disabled={disabled}
                Sign up
              </button> */}
            <a
              className="main-button text-md md:text-lg py-2 px-4 md:px-8 md:py-3 w-auto block"
              href="https://967gb74hmbf.typeform.com/to/nVtqlzdj"
              target="_blank"
              rel="noreferrer"
            >
              Request Access
            </a>
          </div>
          <div className="p-4 md:block hidden">
            <p className="md:block my-4 text-sm">
              If you're interested in {IDE_NAME} but it's not a fit right now,
              you can also subscribe to email updates.
            </p>
            <form
              action="https://gmail.us6.list-manage.com/subscribe/post?u=530176c3897958e56302043ed&amp;id=cb045d5e14"
              className="text-center"
              method="post"
              name="mc-embedded-subscribe-form"
              target="_blank"
              novalidate
            >
              <div className="flex flex-col lg:flex-row">
                <input
                  type="email"
                  defaultValue=""
                  name="EMAIL"
                  placeholder="Email address"
                  required
                  className="py-1.5 px-2.5 rounded-md text-sm lg:mr-2 flex-grow"
                  style={{
                    background: "#222",
                    border: "solid 1px #555",
                  }}
                />
                <div
                  style={{ position: "absolute", left: "-5000px" }}
                  aria-hidden="true"
                >
                  <input
                    type="text"
                    name="b_530176c3897958e56302043ed_cb045d5e14"
                    tabindex="-1"
                    value=""
                  />
                </div>
                <button
                  className="mt-2 lg:mt-0 main-button from-gray-200 to-gray-300 text-gray-600"
                  type="submit"
                >
                  Subscribe
                </button>
              </div>
            </form>
          </div>
        </div>

        <div className="">
          <Title>Before you sign up...</Title>
          <p>
            {IDE_NAME} is still in early beta. We're releasing it now because we
            use it every day ourselves, and realized we were getting more than
            $10 of monthly utility from it. That said, there are currently
            several large limitations:
          </p>
          <ul className="thick-list">
            <li>Windows only (Windows 10).</li>
            <li>Go 1.13+ only.</li>
            <li>
              Your project must be module-aware, and consist of a single module,
              located at your project's root folder. I.e.{" "}
              <code>go list -mod=mod -m all</code> must work from your root
              folder.
            </li>
            <li>
              Editing via Vim keys only. Arrow keys, clicking, and scrolling
              don't work in the editor yet.
            </li>
            <li>No project-wide Search/Search and Replace.</li>
            <li>No built-in terminal.</li>
            <li>No WSL support.</li>
            <li>No support for symlinks (undefined behavior).</li>
            <li>
              Slow when opening extremely large workspaces (e.g. the{" "}
              <a href="https://github.com/kubernetes/kubernetes">kubernetes</a>{" "}
              repo).
            </li>
            <li>
              No Vim commands. Importantly, no <code>:%s/search/replace</code>.
              (<code>/search</code> works.)
            </li>
            <li>
              Vim macros work, but when repeated (e.g. <code>200@@</code>) they
              start to slow down.
            </li>
          </ul>
          <p>
            Obviously, these limitations are not part of our eventual vision.
            We're working hard to remove them. But in the meantime, {IDE_NAME}{" "}
            is ready for day-to-day, bread-and-butter Go programming. Everyone
            on the dev team uses it. The following core features are working:
          </p>
          <ul className="thick-list">
            <li>
              Automatically index your code and its dependencies in real-time
            </li>
            <li>Autocomplete (member &amp; keyword)</li>
            <li>Parameter hints (show function signature on call)</li>
            <li>Jump to definition</li>
            <li>
              Auto format on save (with <code>goimports</code>)
            </li>
            <li>Vim keybindings</li>
            <li>
              Debugging with call stack, breakpoints, local variables, and
              watches
            </li>
            <li>Debug tests (package-wide &amp; individual tests)</li>
            <li>Building, navigating between errors</li>
            <li>Fuzzy file picker</li>
            <li>Extremely optimized core</li>
          </ul>
          <p>
            If this sounds compatible with your setup, {IDE_NAME} will allow you
            to start developing Go programs at lightning speed today. We're also{" "}
            <Link to="/roadmap">steadily developing new features</Link>.
          </p>
        </div>
      </div>
    </div>
  );
}

function Roadmap() {
  return (
    <WallOfText title="Roadmap">
      <p>Here are things we're looking to add in the immediate future.</p>
      <ul className="roadmap-list text-gray-400">
        <li>macOS and Linux support.</li>
        <li>Make the editor usable for non-Vim users.</li>
        <li>Git integration.</li>
        <li>A more sophisticated file browser.</li>
        <li>WSL support.</li>
        <li>
          Go to symbol &mdash; like fuzzy file picker, but for names of declared
          identifiers instead of files.
        </li>
        <li>Improved details in autocomplete menu.</li>
        <li>Find all usages.</li>
        <li>Automatic refactoring.</li>
        <li>Support symlinks.</li>
        <li>Support large repositories.</li>
      </ul>
    </WallOfText>
  );
}

/*
function About() {
  return (
    <div>
      <Title>About</Title>
      <p>{IDE_NAME} is created by Brandon Hsiao.</p>
    </div>
  );
}
*/

function PaymentCanceled() {
  return (
    <WallOfText title="Your payment was canceled.">
      <p>
        If you didn't mean to cancel it, you can{" "}
        <Link to="/beta">try again</Link>.
      </p>
      <p>
        If you believe the payment went through and you were charged, please{" "}
        <a href={`mailto:${SUPPORT_EMAIL}`}>email us</a>.
      </p>
      <p>
        Otherwise, <Link to="/">click here</Link> to return to the main page.
      </p>
    </WallOfText>
  );
}

function PaymentSuccess() {
  return (
    <WallOfText title="Your payment went through!">
      <p>Please check your email for the download link and your license key.</p>
      <p>
        If the email doesn't come, please check your spam folder and wait a few
        minutes. If it still doesn't come, please do not purchase a second time
        &mdash; two subscriptions will be created. Instead,{" "}
        <a href={`mailto:${SUPPORT_EMAIL}`}>email us</a>.
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

function App() {
  return (
    <Router>
      <ScrollToTop />

      <Helmet>
        <meta charSet="utf-8" />
        <title>{IDE_NAME}</title>
      </Helmet>

      <div className="p-6 md:p-12 leading-relaxed text-gray-400 w-full lg:max-w-screen-lg lg:mx-auto">
        <div className="mx-auto pb-4 flex justify-between items-center">
          <Link to="/" className="font-bold text-white no-underline">
            {IDE_NAME}
          </Link>
          <div className="flex items-baseline space-x-6">
            <Link
              className="no-underline text-sm font-bold text-gray-300 hidden sm:inline-block"
              to="/philosophy"
            >
              Philosophy
            </Link>
            <Link
              className="no-underline text-sm font-bold text-gray-300 hidden sm:inline-block"
              to="/roadmap"
            >
              Roadmap
            </Link>
            <Link className="main-button" to="/beta">
              Request Access
            </Link>
          </div>
        </div>
        <div className="">
          <Switch>
            <Route path="/download">
              <Download />
            </Route>
            <Route path="/portal">
              <Portal />
            </Route>
            <Route path="/terms">
              <Terms />
            </Route>
            <Route path="/privacy">
              <Privacy />
            </Route>
            <Route path="/philosophy">
              <Philosophy />
            </Route>
            <Route path="/beta">
              <Beta />
            </Route>
            <Route path="/payment-canceled">
              <PaymentCanceled />
            </Route>
            <Route path="/payment-success">
              <PaymentSuccess />
            </Route>
            <Route path="/roadmap">
              <Roadmap />
            </Route>
            <Route exact path="/">
              <Home />
            </Route>
          </Switch>
        </div>
        <div className="text-sm pt-4">
          <div className="lg:max-w-screen-xl lg:mx-auto flex flex-col sm:flex-row justify-between">
            <div className="text-gray-400">
              &copy; {CURRENT_YEAR} {IDE_NAME}
            </div>
            <div className="flex flex-col sm:flex-row space-x-0 sm:space-x-8 mt-2 sm:mt-0">
              <div className="text-left sm:text-right">
                <div className="mt-1">
                  <Link to="/beta" className="text-gray-400 no-underline">
                    Request Access
                  </Link>
                </div>
                <div className="mt-1">
                  <Link to="/philosophy" className="text-gray-400 no-underline">
                    Philosophy
                  </Link>
                </div>
                <div className="mt-1">
                  <Link to="/roadmap" className="text-gray-400 no-underline">
                    Roadmap
                  </Link>
                </div>
              </div>
              <div className="text-left sm:text-right">
                <div className="mt-1">
                  <Link to="/terms" className="text-gray-400 no-underline">
                    Terms of Service
                  </Link>
                </div>
                <div className="mt-1">
                  <Link className="text-gray-400 no-underline" to="/privacy">
                    Privacy Policy
                  </Link>
                </div>
                <div className="mt-1">
                  <a
                    className="text-gray-400 no-underline"
                    href={`mailto:${SUPPORT_EMAIL}`}
                  >
                    Contact
                  </a>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </Router>
  );
}

export default App;
