/** Reports what the launcher gave it, and behaves as its arguments ask. */
public class TestApp {
	public static void main(String[] args) throws Exception {
		System.out.println("version=" + System.getProperty("java.version"));
		System.out.println("classpath=" + System.getProperty("java.class.path"));
		System.out.println("testprop=" + System.getProperty("testprop"));
		System.out.println("workdir=" + System.getProperty("user.dir"));
		for (String arg : args) {
			System.out.println("arg=" + arg);
			if (arg.startsWith("exit:"))
				System.exit(Integer.parseInt(arg.substring(5)));
			if (arg.equals("throw"))
				throw new IllegalStateException("expected by the test");
			if (arg.equals("thread")) {
				Thread thread = new Thread(() -> {
					try {
						Thread.sleep(1500);
					} catch (InterruptedException e) {
						return;
					}
					System.out.println("thread finished");
				});
				thread.start();
			}
		}
		System.out.println("main finished");
	}
}
