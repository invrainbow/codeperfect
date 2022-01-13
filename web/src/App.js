import cx from "classnames";
import React from "react";
import { CopyToClipboard } from "react-copy-to-clipboard";
import { Helmet } from "react-helmet";
import { AiFillCode } from "react-icons/ai";
import { BsArrowRight, BsCodeSquare, BsCpu } from "react-icons/bs";
import {
  FaApple,
  FaLayerGroup,
  FaPalette,
  FaRegClipboard,
  FaRobot,
} from "react-icons/fa";
import { FcCheckmark } from "react-icons/fc";
import { GoPackage } from "react-icons/go";
import { HiArrowNarrowLeft, HiArrowNarrowRight } from "react-icons/hi";
import { ImMagicWand } from "react-icons/im";
import { IoMdSearch } from "react-icons/io";
import { SiVim } from "react-icons/si";
import { VscTools } from "react-icons/vsc";
import {
  BrowserRouter as Router,
  Link,
  Redirect,
  Route,
  Switch,
  useHistory,
  useLocation,
  useParams,
} from "react-router-dom";
import "./index.css";

const SUPPORT_EMAIL = "support@codeperfect95.com";
const BETA_LINK =
  "https://codeperfect95.notion.site/codeperfect95/CodePerfect-macOS-Beta-b899fc159c6341abae382f2b5b744bc5";

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

function Icon({ icon: IconComponent, ...props }) {
  return (
    <span className="relative -top-0.5">
      <IconComponent {...props} />
    </span>
  );
}

function Feature({ bookend, name, className, children, icon, ...props }) {
  return (
    <div
      className={cx(
        className,
        "text-gray-600 p-3 md:p-5 rounded-lg relative",
        "bg-gray-100 transform md:hover:scale-105 ease-in duration-100"
      )}
      {...props}
    >
      <div className="flex flex-col mb-2">
        <div className="text-3xl mb-4 opacity-40">
          <Icon icon={icon} />
        </div>
        <div className="text-lg font-semibold leading-tight">{name}</div>
      </div>
      <div className="opacity-70">{children}</div>
    </div>
  );
}

function Home() {
  const [startDemo, setStartDemo] = React.useState(false);

  return (
    <div className="w-full">
      <div className="mt-8 px-4 sm:mt-24 sm:mb-8 md:mt-24 md:mb-12 lg:max-w-screen-xl lg:mx-auto">
        <div className="leading-tight text-center mb-2 text-4xl md:text-5xl font-bold text-black">
          <span className="inline-block">The Fastest</span>{" "}
          <span className="inline-block">IDE for Go</span>
        </div>
        <div className="mt-4 mb-6 md:mb-20 text-lg text-gray-400 text-center">
          <A
            href={BETA_LINK}
            className="rounded-full bg-gray-100 hover:bg-gray-200 color-gray-400 inline-block text-sm py-1 px-4 no-underline"
          >
            <b className="text-black">NEW:</b> <Icon icon={FaApple} /> macOS now
            in private beta!
          </A>
        </div>
      </div>

      <div
        className={cx(
          "max-w-screen-lg mx-auto gap-8 px-3 items-center justify-center"
        )}
      >
        <div
          className="relative rounded-lg overflow-hidden border border-gray-800 shadow bg-black"
          style={startDemo ? { paddingBottom: "62.5%", top: 0 } : {}}
        >
          {startDemo ? (
            <iframe
              className="absolute top-0 left-0 w-full h-full"
              src="https://www.youtube-nocookie.com/embed/iPCfOoK9fco?autoplay=1"
              title="YouTube video player"
              frameborder="0"
              allow="fullscreen; accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture"
              allowfullscreen
            />
          ) : (
            <div
              className="cursor-pointer relative"
              onClick={() => setStartDemo(true)}
            >
              <div className="absolute w-full h-full flex items-center justify-center z-10">
                <img alt="play" style={{ width: "72px" }} src="/play.png" />
              </div>
              <img className="block relative z-0" src="/demo.png" alt="demo" />
            </div>
          )}
        </div>
      </div>

      <div className="px-4 my-16 md:my-36">
        <div className="text-center font-bold text-3xl mb-8 text-black">
          A full-featured IDE, as fast as Sublime Text.
        </div>
        <div className="mx-auto max-w-screen-lg grid grid-cols-2 sm:grid-cols-3 gap-3 md:grid-cols-4 lg:grid-cols-5 md:gap-5">
          <Feature name="Code intelligence" icon={BsCodeSquare}>
            Autocomplete, go to definition, parameter hints, find usages, and
            more.
          </Feature>

          <Feature name="Built for speed" icon={BsCpu}>
            Written in C++, no Electron. Runs at 144 FPS. Keystrokes are
            instant.
          </Feature>

          <Feature name="Build and debug" icon={VscTools}>
            Edit, build and debug in one workflow, one app, one place.
          </Feature>

          <Feature name="Vim keybindings" icon={SiVim}>
            Vim keybindings work natively out of the box, the full feature set.
          </Feature>

          <Feature name="Instant fuzzy search" icon={IoMdSearch}>
            Works on files, symbols, commands, and completions.
          </Feature>

          <Feature name="Automatic refactoring" icon={ImMagicWand}>
            Rename, move, and generate code automatically.
          </Feature>

          <Feature name="Automatic import" icon={GoPackage}>
            Pull in libraries you need without moving your cursor.
          </Feature>

          <Feature name="Postfix macros" icon={FaRobot}>
            Generate code with macros that work intelligently on your Go
            expressions.
          </Feature>
          <Feature name="Native support for interfaces" icon={FaLayerGroup}>
            Navigate and generate interfaces in a few keystrokes.
          </Feature>
          <Feature name="Command Palette" icon={FaPalette}>
            Press ⌘K to run any command or action inside CodePerfect.
          </Feature>
        </div>
      </div>

      <div className="my-16 md:my-36 lg:mt-52 lg:mb-48 lg:rounded-md bg-black max-w-screen-xl mx-auto flex">
        <div className="lg:w-2/5 p-8 md:p-16">
          <div className="font-bold text-3xl text-white leading-snug">
            <div>Ready to try it out?</div>
            <div>Help test our macOS beta.</div>
          </div>
          <div className="text-lg text-white opacity-80 mt-4 mb-6">
            <p>
              Our macOS release is in private beta. We're looking for testers to
              help us improve the product.
            </p>
          </div>
          <div>
            <A
              className="button main-button text-lg bg-white text-black hover:bg-white hover:text-black"
              href={BETA_LINK}
            >
              Request Access
            </A>
          </div>
        </div>
        <div className="flex-1 pl-12 relative hidden lg:block">
          <div
            style={{ transform: "translate(0, -47.5%)" }}
            className="absolute top-1/2 -left-16"
          >
            <img className="w-full h-auto" src="/beta.png" alt="beta" />
          </div>
        </div>
      </div>
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
              src="/logo.png"
            />
            <span className="hidden md:inline-block">CodePerfect 95</span>
          </Link>
          <div className="flex items-baseline space-x-6">
            <A className="button main-button" role="button" href={BETA_LINK}>
              Join Beta
            </A>
          </div>
        </div>
        <div>
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
                <A className="text-gray-500 no-underline" href={BETA_LINK}>
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
                  href="https://codeperfect95.notion.site/codeperfect95/CodePerfect-95-Changelog-dcedf41014ef4de79690a5b5b54ebb33"
                >
                  Changelog
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
